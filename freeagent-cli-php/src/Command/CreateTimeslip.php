<?php

namespace App\Command;

use App\Config;
use App\ProviderAuthenticator;
use Symfony\Component\Console\Command\Command;
use Symfony\Component\Console\Input\InputArgument;
use Symfony\Component\Console\Input\InputInterface;
use Symfony\Component\Console\Output\OutputInterface;

class CreateTimeslip extends Command
{
    protected static $defaultName = 'create-timeslip';
    protected $config;

    public function __construct(Config $config)
    {
        $this->config = $config;
        parent::__construct();
    }

    protected function configure()
    {
        $this->addArgument('taskId', InputArgument::REQUIRED);
        $this->addArgument('comment', InputArgument::REQUIRED);
    }

    protected function execute(InputInterface $input, OutputInterface $output)
    {
        $providerAuthenticator = new ProviderAuthenticator($this->config);
        $provider = $providerAuthenticator->getProvider();
        $accessToken = $providerAuthenticator->getAccessToken();

        $taskId = $input->getArgument('taskId');
        $comment = $input->getArgument('comment');

        // Load associated task
        $taskRequest = $provider->getAuthenticatedRequest(
            'GET',
            'tasks/' . $taskId,
            $accessToken
        );
        $taskResponse = $provider->getParsedResponse($taskRequest);

        $createTimeslipRequestData = ['timeslip' => [
            'task' => $taskResponse['task']['url'],
            'user' => $this->config->timeslipUser,
            'project' => $taskResponse['task']['project'],
            'dated_on' => date('Y-m-d'),
            'hours' => 0,
            'comment' => $comment
        ]];
        $createTimeslipRequest = $provider->getAuthenticatedRequest(
            'POST',
            'timeslips',
            $accessToken,
            [
                'headers' => [
                    'Content-Type' => 'application/json',
                ],
                'body' => json_encode($createTimeslipRequestData)
            ]
        );
        $createTimeslipResponse = $provider->getParsedResponse($createTimeslipRequest);
        $startTimerRequest = $provider->getAuthenticatedRequest(
            'POST',
            $createTimeslipResponse['timeslip']['url'] . '/timer',
            $accessToken
        );
        $provider->getResponse($startTimerRequest);

        return Command::SUCCESS;
    }
}