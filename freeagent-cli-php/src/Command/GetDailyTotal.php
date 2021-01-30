<?php

namespace App\Command;

use App\Config;
use App\ProviderAuthenticator;
use Symfony\Component\Console\Command\Command;
use Symfony\Component\Console\Input\InputInterface;
use Symfony\Component\Console\Output\OutputInterface;

class GetDailyTotal extends Command
{
    protected static $defaultName = 'get-daily-total';

    protected $config;

    public function __construct(Config $config)
    {
        $this->config = $config;
        parent::__construct();
    }

    protected function execute(InputInterface $input, OutputInterface $output)
    {
        $providerAuthenticator = new ProviderAuthenticator($this->config);
        $provider = $providerAuthenticator->getProvider();
        $accessToken = $providerAuthenticator->getAccessToken();

        $billedAmount = 0;
        $today = date('Y-m-d');

        $timeslipsRequest = $provider->getAuthenticatedRequest(
            'GET',
            sprintf(
                'timeslips?from_date=%s&to_date=%s&view=%s',
                $today,
                $today,
                'all' // or running
            ),
            $accessToken
        );
        $timeslipsResponse = $provider->getParsedResponse($timeslipsRequest);
        foreach ($timeslipsResponse['timeslips'] as $timeslip) {

            if (isset($timeslip['timer'])) {
                $timerStart = new \DateTime($timeslip['timer']['start_from']);
                $now = new \DateTime();
                $interval = $now->diff($timerStart);
                $timeslip['hours'] = $interval->h + ($interval->i / 60) + ($interval->s / 3600);
            }

            $urlParts = explode("/", $timeslip['url']);
            $timeslip['id'] = array_pop($urlParts);

            // Load associated task
            $taskRequest = $provider->getAuthenticatedRequest(
                'GET',
                $timeslip['task'],
                $accessToken
            );
            $taskResponse = $provider->getParsedResponse($taskRequest);
            $timeslip['task'] = $taskResponse['task'];

            if ($timeslip['task']['billing_period'] == 'day') {
                // Load associated project
                $projectRequest = $provider->getAuthenticatedRequest(
                    'GET',
                    $timeslip['project'],
                    $accessToken
                );
                $projectResponse = $provider->getParsedResponse($projectRequest);
                $timeslip['project'] = $projectResponse['project'];

                $billedAmount +=
                    ($timeslip['task']['billing_rate'] / $timeslip['project']['hours_per_day'])
                    *
                    ($timeslip['hours']);
            } else {
                $billedAmount +=
                    $timeslip['hours'] * $timeslip['task']['billing_rate'];
            }
        }
        $output->write(number_format($billedAmount, 2));
        return Command::SUCCESS;
    }
}