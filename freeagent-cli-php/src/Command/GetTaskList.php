<?php

namespace App\Command;

use App\ProviderAuthenticator;
use App\Config;
use Symfony\Component\Console\Command\Command;
use Symfony\Component\Console\Input\InputInterface;
use Symfony\Component\Console\Output\OutputInterface;

class GetTaskList extends Command
{
    protected static $defaultName = 'get-task-list';

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

        $tasks = [];
        $projects = [];
        $tasksRequest = $provider->getAuthenticatedRequest(
            'GET',
            'tasks',
            $accessToken
        );
        $tasksResponse = $provider->getParsedResponse($tasksRequest);
        foreach($tasksResponse['tasks'] as $task) {
            if ($task['status'] != "Active")
                continue;

            $taskUrlParts = explode("/", $task['url']);
            $taskId = array_pop($taskUrlParts);

            // Load associated project
            if (isset($projects[$task['project']])) {
                $task['project'] = $projects[$task['project']];
            } else {
                $projectRequest = $provider->getAuthenticatedRequest(
                    'GET',
                    $task['project'],
                    $accessToken
                );
                $projectResponse = $provider->getParsedResponse($projectRequest);
                // memoize
                $projects[$task['project']] = $projectResponse['project'];
                $task['project'] = $projectResponse['project'];
            }

            // Created formatted string for output
            $tasks[$taskId] = sprintf(
                "%s: %s - %s (£%s per %s)",
                $task['project']['contact_name'],
                $task['project']['name'],
                $task['name'],
                number_format($task['billing_rate'], 2),
                $task['billing_period']
            );
        }

        asort($tasks);
        foreach($tasks as $id => $task) {
            $output->writeln($id . "\t" . $task);
        }
        return Command::SUCCESS;
    }
}