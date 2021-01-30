<?php

namespace App\Command;

use App\Config;
use App\ProviderAuthenticator;
use Symfony\Component\Console\Command\Command;
use Symfony\Component\Console\Input\InputArgument;
use Symfony\Component\Console\Input\InputInterface;
use Symfony\Component\Console\Output\OutputInterface;

class StartTimer extends Command
{
    protected static $defaultName = 'start-timer';
    protected $config;

    public function __construct(Config $config)
    {
        $this->config = $config;
        parent::__construct();
    }

    protected function configure()
    {
        $this->addArgument('timeslipId', InputArgument::REQUIRED);
    }

    protected function execute(InputInterface $input, OutputInterface $output)
    {
        $providerAuthenticator = new ProviderAuthenticator($this->config);
        $provider = $providerAuthenticator->getProvider();
        $accessToken = $providerAuthenticator->getAccessToken();

        $timeslipId = $input->getArgument('timeslipId');
        $request = $provider->getAuthenticatedRequest(
            'POST',
            sprintf('timeslips/%s/timer', $timeslipId),
            $accessToken
        );
        $response = $provider->getResponse($request);
        if ($response->getStatusCode() == 200)
            return Command::SUCCESS;
        else
            return Command::FAILURE;
    }
}