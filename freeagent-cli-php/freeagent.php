#!/usr/bin/env php
<?php

require __DIR__.'/vendor/autoload.php';

use App\Config;
use Symfony\Component\Console\Application;

$config = new Config();
$config->load($_SERVER['HOME'] . "/.config/freeagent-cli-php.json");

$application = new Application();
$application->setAutoExit(false);
$application->add(new App\Command\CreateTimeslip($config));
$application->add(new App\Command\GetDailyTotal($config));
$application->add(new App\Command\GetRunningTimers($config));
$application->add(new App\Command\GetTaskList($config));
$application->add(new App\Command\GetTimeslipList($config));
$application->add(new App\Command\Login($config));
$application->add(new App\Command\StartTimer($config));
$application->add(new App\Command\StopTimer($config));
$returnCode = $application->run();
$config->save($_SERVER['HOME'] . "/.config/freeagent-cli-php.json");
exit($returnCode);