<?php

namespace DDTrace;

use DDTrace\Contracts\Scope as OpenTracingScope;

final class Scope implements OpenTracingScope
{
    /**
     * @var SpanInterface
     */
    private $span;

    /**
     * @var ScopeManager
     */
    private $scopeManager;

    /**
     * @var bool
     */
    private $finishSpanOnClose;

    public function __construct(ScopeManager $scopeManager, SpanInterface $span, $finishSpanOnClose)
    {
        $this->scopeManager = $scopeManager;
        $this->span = $span;
        $this->finishSpanOnClose = $finishSpanOnClose;
    }

    /**
     * {@inheritdoc}
     */
    public function close()
    {
        if ($this->finishSpanOnClose) {
            $this->span->finish();
        }

        $this->scopeManager->deactivate($this);
    }

    /**
     * {@inheritdoc}
     *
     * @return SpanInterface
     */
    public function getSpan()
    {
        return $this->span;
    }
}
