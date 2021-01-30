<?php

namespace App\Command;

use App\ProviderAuthenticator;
use App\Config;
use Symfony\Component\Console\Command\Command;
use Symfony\Component\Console\Input\InputArgument;
use Symfony\Component\Console\Input\InputInterface;
use Symfony\Component\Console\Output\OutputInterface;

class GetTimeslipList extends Command
{
    protected static $defaultName = 'get-timeslip-list';

    protected $config;

    public function __construct(Config $config)
    {
        $this->config = $config;
        parent::__construct();
    }

    protected function configure()
    {
        $this->addArgument('taskId', InputArgument::REQUIRED);
    }

    protected function execute(InputInterface $input, OutputInterface $output)
    {
        $providerAuthenticator = new ProviderAuthenticator($this->config);
        $provider = $providerAuthenticator->getProvider();
        $accessToken = $providerAuthenticator->getAccessToken();

        $taskId = $input->getArgument('taskId');

        $timeslips = [];

        $timeslipsRequest = $provider->getAuthenticatedRequest(
            'GET',
            'timeslips?task=' . $taskId,
            $accessToken
        );
        $timeslipsResponse = $provider->getParsedResponse($timeslipsRequest);
        foreach($timeslipsResponse['timeslips'] as $timeslip) {
            $timeslipUrlParts = explode("/", $timeslip['url']);
            $timeslipId = array_pop($timeslipUrlParts);

            if (isset($timeslip['timer'])) {
                $timerStart = new \DateTime($timeslip['timer']['start_from']);
                $now = new \DateTime();
                $interval = $now->diff($timerStart);
                $timeslip['hours'] = $interval->h + ($interval->i / 60) + ($interval->s / 3600);
            }

            $taskUrlParts = explode("/", $timeslip['task']);
            $taskId = array_pop($taskUrlParts);

            $timeslips[$timeslipId] = sprintf(
                "[%s] %s  %s:%s\t%s%s",
                $timeslip['dated_on'],
                str_pad(substr($timeslip['comment'] ?? '', 0, 50) .  (strlen($timeslip['comment'] ?? '') > 50 ? "..." : ""), 54),
                str_pad(floor($timeslip['hours']), 2, " ", STR_PAD_LEFT),
                str_pad(round(60 * ($timeslip['hours'] - floor($timeslip['hours']))), 2, "0", STR_PAD_LEFT),
                $taskId,
                isset($timeslip['timer']) ? "\tR" : ""
            );
        }

        foreach($timeslips as $id => $timeslip) {
            $output->writeln($id . "\t" . $timeslip);
        }
        return Command::SUCCESS;
    }
}