<?php

namespace DDTrace\Bridge;

require_once __DIR__ . '/dd_autoload.php';

use DDTrace\Encoders\Json;
use DDTrace\GlobalTracer;
use DDTrace\Integrations\IntegrationsLoader;
use DDTrace\Tracer;
use DDTrace\Transport\Http;

$tracer = new Tracer(new Http(new Json()));

GlobalTracer::set($tracer);

// The root scope WILL not be created here, but our tracer will provide a way to flush the root span.

register_shutdown_function(function() use ($tracer) {
    $tracer->getScopeManager()->closeAllHostRoots();
    $tracer->flush();
});

IntegrationsLoader::load();
