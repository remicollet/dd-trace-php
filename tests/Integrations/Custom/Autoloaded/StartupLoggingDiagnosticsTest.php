<?php

namespace DDTrace\Tests\Integrations\Custom\Autoloaded;

use DDTrace\Tests\Common\WebFrameworkTestCase;
use DDTrace\Tests\Frameworks\Util\Request\GetSpec;

final class StartupLoggingDiagnosticsTest extends WebFrameworkTestCase
{
    const IS_SANDBOX = true;

    protected static function getAppIndexScript()
    {
        return __DIR__ . '/../../../Frameworks/Custom/Version_Autoloaded/public/index.php';
    }

    private static function getAppErrorLog()
    {
        return \dirname(static::getAppIndexScript()) . '/startup_logging.log';
    }

    protected static function getEnvs()
    {
        return array_merge(parent::getEnvs(), [
            'DD_TRACE_DEBUG' => true, // Emits diagnostic messages
            'DD_AGENT_HOST' => 'invalid_host', // Will fail diagnostic check
        ]);
    }

    protected function setUp()
    {
        parent::setUp();

        // clear out any previous logs
        $log = self::getAppErrorLog();
        @\unlink($log);
        \touch($log);
    }

    protected static function getInis()
    {
        return array_merge(parent::getInis(), [
            'error_log' => self::getAppErrorLog(),
            'ddtrace.request_init_hook' => '/foo/invalid.php', // Will fail diagnostic check
        ]);
    }

    public function testDiagnosticChecksLoggedWhenDebugModeEnabled()
    {
        $this->tracesFromWebRequest(function () {
            $spec = GetSpec::create('Startup logs diagnostics test', '/simple');
            $this->call($spec);
        });

        $contents = \file_get_contents(self::getAppErrorLog());

        self::assertContains('DATADOG TRACER DIAGNOSTICS - agent_error:', $contents);
        self::assertContains('DATADOG TRACER DIAGNOSTICS - ddtrace.request_init_hook_reachable:', $contents);
    }
}
