//@ sourceURL=Gputop.js

// https://google.github.io/styleguide/javascriptguide.xml

Object.size = function(obj) {
    var size = 0, key;
    for (key in obj) {
        if (obj.hasOwnProperty(key)) size++;
    }
    return size;
};

//------------------------------ Protobuffer Init ----------------------------------------

if (typeof dcodeIO === 'undefined' || !dcodeIO.ProtoBuf) {
    throw(new Error("ProtoBuf.js is not present. Please see www/index.html for manual setup instructions."));
}
// Initialize ProtoBuf.js
var ProtoBuf = dcodeIO.ProtoBuf;

var proto_builder = ProtoBuf.loadProtoFile("./proto/gputop.proto");

//------------------------------ METRIC ---------------------------------------------------
function Counter () {
    this.idx_ = -1;    
    this.emscripten_idx_ = -1;
    this.available_ = false;
    this.xml_ = "<xml/>";
}

//------------------------------ METRIC ---------------------------------------------------
function Metric () {
    this.guid_ = "undefined";
    this.xml_ = "<xml/>";
    this.supported_ = false;
    this.counters_ = {};
    this.metric_set_ = 0;
}

Metric.prototype.print = function() {
    gputop_ui.syslog(this.guid_);
}

//------------------------------ GPUTOP ---------------------------------------------------
function Gputop () {
    this.map_metrics_ = {};
   
    this.is_connected_ = false;
    // Gputop generic configuration
    this.config_ = {
        url_path: 'localhost',
        uri_port: 7890,
        architecture: 'ukn'
    }

    // Initialize protobuffers
    this.builder_ = proto_builder.build("gputop");

    // Queries
    this.all_oa_queries_ = [];
    this.current_oa_query = 0;
    this.query_id_ = 0;
    this.query_id_next_ = 1;
    this.query_id_current_oa_ = 0;
    this.query_handles_ = [];
}

Gputop.prototype.get_metrics_xml = function() {
    return this.metrics_xml_;
}

Gputop.prototype.get_map_metric = function(guid){
    var metric;    
    if (guid in this.map_metrics_) {
        metric = this.map_metrics_[guid];
    } else {
        metric = new Metric();
        metric.guid_ = guid;        
        metric.metric_set_ = Object.keys(this.map_metrics_[guid]).length;
        this.map_metrics_[guid] = metric;
    }
    return metric;
}

function gputop_read_metrics_set() {
    try { 
        var $set = $(this);
        var title = $set.attr("name");
        var guid = $set.attr("guid");
    
        gputop_ui.syslog('Found metric ' + title);
    
        var metric = gputop.get_map_metric(guid);    
        metric.xml_ = $set;
        metric.print();  
        gputop.map_metrics_[guid] = metric;
    } catch (e) {
        gputop_ui.syslog("Catch parsing " + e);
    }
} // read_metrics_set

Gputop.prototype.load_xml_metrics = function(xml) {
    this.metrics_xml_ = xml;
    $(xml).find("set").each(gputop_read_metrics_set);

    gputop_ui.load_metrics_panel(function() {
        //TODO(@sergioamr) How do you get the first entry in JS?
        for(var guid in gputop.map_metrics_) {
            gputop.open_oa_query_for_trace(guid);
            return;
        }
    });
}

Gputop.prototype.load_oa_queries = function(architecture) {    
    console.log("Loading " + architecture);
    this.config_.architecture = architecture;

    // read counters from xml file and populate the website
    var xml_name = "xml/oa-"+ architecture +".xml";
    $.get(xml_name, this.load_xml_metrics);
}

Gputop.prototype.open_oa_query_for_trace = function(guid) {
    if (guid == undefined) {
        gputop_ui.show_alert("GUID missing while trying to opening query","alert-danger");
        return;
    }
    
    gputop_ui.syslog("Launch query GUID " + guid);
    var metric = this.get_map_metric(guid);

    if (metric.supported_ == false) {
        gputop_ui.show_alert("Metric "+guid+" not supported","alert-danger");        
        return;
    }
    
    var oa_query = new this.builder_.OAQueryInfo();
    oa_query.guid = guid;
    oa_query.metric_set = metric.metric_set_;    /* 3D test */    

    /* The timestamp for HSW+ increments every 80ns
     *
     * The period_exponent gives a sampling period as follows:
     *   sample_period = 80ns * 2^(period_exponent + 1)
     *
     * The overflow period for Haswell can be calculated as:
     *
     * 2^32 / (n_eus * max_gen_freq * 2)
     * (E.g. 40 EUs @ 1GHz = ~53ms)
     *
     * We currently sample ~ every 10 milliseconds...
     */

    this.query_id_ = this.query_id_next_++;

    var msg = new this.builder_.Request();
    msg.uuid = this.generate_uuid();

    var open = new this.builder_.OpenQuery();

    oa_query.period_exponent = 16 ;

    open.id = this.query_id_; // oa_query ID
    open.overwrite = false;   /* don't overwrite old samples */
    open.live_updates = true; /* send live updates */
                         /* nanoseconds of aggregation
				          * i.e. request updates from the worker
				          * as values that have been aggregated
				          * over this duration */
    open.oa_query = oa_query;

    _gputop_webworker_on_open_oa_query(
        this.query_id_,
        oa_query.metric_set,
        oa_query.period_exponent,
        open.overwrite, /* don't overwrite old samples */
		      100000000, /* nanoseconds of aggregation
				          * i.e. request updates from the worker
				          * as values that have been aggregated
				          * over this duration */
        open.live_updates /* send live updates */);


    msg.open_query = open;
    msg.encode();
    this.socket_.send(msg.toArrayBuffer());

    gputop_ui.syslog("Sent: Request "+msg.uuid);

    this.query_handles_.push(this.query_id);

    this.current_oa_query = oa_query;
    this.current_oa_query_id = this.query_id_;
}

Gputop.prototype.get_server_url = function() {
    return this.config_.url_path+':'+this.config_.uri_port;
}

Gputop.prototype.get_websocket_url = function() {
    return 'ws://'+this.get_server_url()+'/gputop/';
}

/* Native compiled Javascript from emscripten to process the counters data */
Gputop.prototype.get_gputop_native_js = function() {
    return 'http://'+this.get_server_url()+'/gputop-web-v2.js';
}

Gputop.prototype.generate_uuid = function()
{
    /* Concise uuid generator from:
     * http://stackoverflow.com/questions/105034/create-guid-uuid-in-javascript
     */
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
    	var r = Math.random()*16|0, v = c == 'x' ? r : (r&0x3|0x8);
    	return v.toString(16);
    });
}

Gputop.prototype.request_features = function() {
    if (this.socket_.readyState == WebSocket.OPEN) {
        var msg = new this.builder_.Request();

        msg.uuid = this.generate_uuid();
        msg.get_features = true;

        msg.encode();
        this.socket_.send(msg.toArrayBuffer());
        gputop_ui.syslog("Sent: Request "+msg.uuid);
    } else {
        gputop_ui.syslog("Not connected");
    }
}

Gputop.prototype.metric_supported = function(element, index, array){    
    var metric = gputop.get_map_metric(element);
    metric.supported_ = true; 
    metric.print();
}

Gputop.prototype.process_features = function(features){
    gputop_ui.display_features(features);

    features.supported_oa_query_guids.forEach(this.metric_supported);

    var di = features.devinfo;   
    _update_features(di.devid, di.n_eus,  di.n_eu_slices,
        di.n_eu_sub_slices, di.eu_threads_count, di.subslice_mask,
        di.slice_mask);
}

Gputop.prototype.get_socket = function(websocket_url) {
    var socket = new WebSocket( websocket_url);
    socket.binaryType = "arraybuffer"; // We are talking binary

    socket.onopen = function() {
        gputop_ui.syslog("Connected");
        gputop_ui.show_alert("Succesfully connected to GPUTOP","alert-info");
        gputop.request_features();
        gputop.is_connected_ = true;
    };

    socket.onclose = function() {
        gputop_ui.syslog("Disconnected");                       
        gputop_ui.show_alert("Failed connecting to GPUTOP <p\>Retry in 5 seconds","alert-danger");
        setTimeout(function() { // this will automatically close the alert and remove this if the users doesnt close it in 5 secs
            gputop.connect();
        }, 5000);
        
        gputop.is_connected_ = false;
    };

    socket.onmessage = function(evt) {
        try {
            var msg_type = new Uint8Array(evt.data, 0);
            var data = new Uint8Array(evt.data, 8);

            switch(msg_type[0]) {
                case 1: /* WS_MESSAGE_PERF */
                    var id = new Uint16Array(evt.data, 4, 1);
                    // Included in webworker
                    //handle_perf_message(id, data);
                break;
                case 2: /* WS_MESSAGE_PROTOBUF */
                    var msg = gputop.builder_.Message.decode(data);
                    if (msg.features != undefined) {
                        gputop_ui.syslog("Features: "+msg.features.get_cpu_model());
                        gputop.process_features(msg.features);
                    }
                    if (msg.log != undefined) {
                        var entries = msg.log.entries;
                        entries.forEach(function(entry) {
                            gputop_ui.log(entry.log_level, entry.log_message);
                        });
                    }

                break;
                case 3: /* WS_MESSAGE_I915_PERF */
                    var id = new Uint16Array(evt.data, 4, 1);
                    var dataPtr = Module._malloc(data.length);
                    var dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, data.length);
                    dataHeap.set(data);
                    _handle_i915_perf_message(id, dataHeap.byteOffset, data.length);
                    Module._free(dataHeap.byteOffset);
                break;
            }
        } catch (err) {
            log.value += "Error: "+err+"\n";
            console.log("Error: "+err);
        }
    };

    return socket;
}

// Connect to the socket for transactions
Gputop.prototype.connect = function() {
    var websocket_url = this.get_websocket_url();
    gputop_ui.syslog('Connecting to port ' + websocket_url);
    //----------------- Data transactions ----------------------
    this.socket_ = this.get_socket(websocket_url);
}

Gputop.prototype.dispose = function() {
};

var gputop = new Gputop();