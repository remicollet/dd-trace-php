<?php

namespace DDTrace\Tests\Integrations\Symfony\V4_4;

use DDTrace\Tests\Common\SpanAssertion;
use DDTrace\Tests\Common\WebFrameworkTestCase;
use DDTrace\Tests\Frameworks\Util\Request\RequestSpec;

class CommonScenariosSandboxedTest extends WebFrameworkTestCase
{
    const IS_SANDBOX = true;

    protected static function getAppIndexScript()
    {
        return __DIR__ . '/../../../Frameworks/Symfony/Version_4_4/public/index.php';
    }

    protected static function getEnvs()
    {
        return array_merge(parent::getEnvs(), [
            'DD_TRACE_DEBUG' => 'true',
            'DD_SERVICE' => 'test_symfony_44',
        ]);
    }

    /**
     * @dataProvider provideSpecs
     * @param RequestSpec $spec
     * @param array $spanExpectations
     * @throws \Exception
     */
    public function testScenario(RequestSpec $spec, array $spanExpectations)
    {
        $traces = $this->tracesFromWebRequest(function () use ($spec) {
            $this->call($spec);
        });

        $this->assertFlameGraph($traces, $spanExpectations);
    }

    public function provideSpecs()
    {
        return $this->buildDataProvider(
            [
                'A simple GET request returning a string' => [
                    SpanAssertion::build(
                        'symfony.request',
                        'test_symfony_44',
                        'web',
                        'simple'
                    )
                        ->withExactTags([
                            'symfony.route.action' => 'App\Controller\CommonScenariosController@simpleAction',
                            'symfony.route.name' => 'simple',
                            'http.method' => 'GET',
                            'http.url' => 'http://localhost:9999/simple',
                            'http.status_code' => '200',
                        ])
                        ->withChildren([
                            SpanAssertion::exists('symfony.kernel.handle')
                                ->withChildren([
                                    SpanAssertion::exists('symfony.kernel.request'),
                                    SpanAssertion::exists('symfony.kernel.controller'),
                                    SpanAssertion::exists('symfony.kernel.controller_arguments'),
                                    SpanAssertion::exists('symfony.kernel.response'),
                                    SpanAssertion::exists('symfony.kernel.finish_request'),
                                ]),
                            SpanAssertion::exists('symfony.kernel.terminate')->skipIf(!static::IS_SANDBOX),
                        ]),
                    SpanAssertion::exists('symfony.kernel.terminate')->skipIf(static::IS_SANDBOX),
                ],
                'A simple GET request with a view' => [
                    SpanAssertion::build(
                        'symfony.request',
                        'test_symfony_44',
                        'web',
                        'simple_view'
                    )
                        ->withExactTags([
                            'symfony.route.action' => 'App\Controller\CommonScenariosController@simpleViewAction',
                            'symfony.route.name' => 'simple_view',
                            'http.method' => 'GET',
                            'http.url' => 'http://localhost:9999/simple_view',
                            'http.status_code' => '200',
                        ])
                        ->withChildren([
                            SpanAssertion::exists('symfony.kernel.handle')
                                ->withChildren([
                                    SpanAssertion::exists('symfony.kernel.request'),
                                    SpanAssertion::exists('symfony.kernel.controller'),
                                    SpanAssertion::exists('symfony.kernel.controller_arguments'),
                                    SpanAssertion::build(
                                        'symfony.templating.render',
                                        'test_symfony_44',
                                        'web',
                                        'Twig\Environment twig_template.html.twig'
                                    )
                                        ->withExactTags([
                                        ]),
                                    SpanAssertion::exists('symfony.kernel.response'),
                                    SpanAssertion::exists('symfony.kernel.finish_request'),
                                ]),
                            SpanAssertion::exists('symfony.kernel.terminate')->skipIf(!static::IS_SANDBOX),
                        ]),
                    SpanAssertion::exists('symfony.kernel.terminate')->skipIf(static::IS_SANDBOX),
                ],
                'A GET request with an exception' => [
                    SpanAssertion::build(
                        'symfony.request',
                        'test_symfony_44',
                        'web',
                        'error'
                    )
                        ->withExactTags([
                            'symfony.route.action' => 'App\Controller\CommonScenariosController@errorAction',
                            'symfony.route.name' => 'error',
                            'http.method' => 'GET',
                            'http.url' => 'http://localhost:9999/error',
                            'http.status_code' => '500',
                        ])
                        ->setError('Exception', 'An exception occurred')
                        ->withExistingTagsNames(['error.stack'])
                        ->withChildren([
                            SpanAssertion::exists('symfony.kernel.handle')
                                ->withChildren([
                                    SpanAssertion::exists('symfony.kernel.request'),
                                    SpanAssertion::exists('symfony.kernel.controller'),
                                    SpanAssertion::exists('symfony.kernel.controller_arguments'),
                                    SpanAssertion::exists('symfony.kernel.handleException')
                                        ->withChildren([
                                            SpanAssertion::exists('symfony.kernel.exception'),
                                            SpanAssertion::exists('symfony.kernel.response')->withChildren([
                                                SpanAssertion::exists('symfony.templating.render')
                                            ]),
                                            SpanAssertion::exists('symfony.kernel.finish_request'),
                                        ]),
                                ]),
                            SpanAssertion::exists('symfony.kernel.terminate'),
                        ]),
                ],
            ]
        );
    }
}
