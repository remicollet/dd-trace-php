<?php

if (!isset($scope)) {
    return null;
}

$thrown = null;
$result = null;
/** @var DDTrace\Contracts\Scope $scope */
$span = $scope->getSpan();
try {
    $result = dd_trace_forward_call();
    if (isset($afterResult)) {
        $afterResult($result, $span, $scope);
    }
} catch (\Exception $ex) {
    $thrown = $ex;
    $span->setError($ex);
}

$scope->close();
if ($thrown) {
    throw $thrown;
}

return $result;
