<?php

namespace App\Command;

use App\ProviderAuthenticator;
use App\Config;
use Symfony\Component\Console\Command\Command;
use Symfony\Component\Console\Input\InputArgument;
use Symfony\Component\Console\Input\InputInterface;
use Symfony\Component\Console\Output\OutputInterface;

class Login extends Command
{
    // the name of the command (the part after "bin/console")
    protected static $defaultName = 'login';

    protected $config;

    protected $oauth2state;
    protected $authCode;

    public function __construct(Config $config)
    {
        $this->config = $config;
        parent::__construct();
    }

    protected function configure()
    {
        $this
            ->addArgument('clientId', InputArgument::REQUIRED)
            ->addArgument('clientSecret', InputArgument::REQUIRED)
            ->addArgument('timeslipUser', InputArgument::REQUIRED)
            ->addArgument('environment', InputArgument::OPTIONAL, '', 'sandbox');
    }

    protected function execute(InputInterface $input, OutputInterface $output)
    {
        $this->config->clientId = $input->getArgument('clientId');
        $this->config->clientSecret = $input->getArgument('clientSecret');
        $this->config->environment = $input->getArgument('environment');
        $this->config->timeslipUser = $input->getArgument('timeslipUser');

        $providerAuthenticator = new ProviderAuthenticator($this->config);
        return $providerAuthenticator->doFreshLogin($output) ? Command::SUCCESS : Command::FAILURE;
    }
}