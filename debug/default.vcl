vcl 4.1;

import querymodifier;
import std;

backend default {
	.host = "nginx";
	.port = "80";
}

sub vcl_recv {
	std.syslog(180, "RECV: " + req.http.host + req.url);
	set req.url = querymodifier.modifyparams(req.url, "ts,v,cacheFix,date", true);
}
