<?php

namespace App;

use App\OAuth\Provider\FreeAgent as FreeAgentProvider;
use League\OAuth2\Client\Token\AccessToken;
use Symfony\Component\Console\Output\OutputInterface;

class ProviderAuthenticator
{
    public $provider;

    protected $config;

    /**
     * Holds a random string to validate the incoming auth code
     */
    protected $oauth2state;

    public function __construct(Config $config)
    {
        $this->config = $config;
    }

    public function getProvider()
    {
        if (empty($this->config->clientId) || empty($this->config->clientSecret)) {
            throw new \Exception("Cannot continue: no client ID or client secret set");
        }

        $oauthProviderConfig = [
            'clientId'     => $this->config->clientId,
            'clientSecret' => $this->config->clientSecret,
            'redirectUri'  => 'http://127.0.0.1:12423/',
            'sandbox'      => $this->config->environment == "sandbox"
        ];

        return new FreeAgentProvider($oauthProviderConfig);
    }

    public function getAccessToken()
    {
        $accessTokenData = $this->config->accessToken;
        $accessToken = new AccessToken($accessTokenData);

        // Automatically refresh token if it's expired
        if ($accessToken->hasExpired()) {
            $provider = $this->getProvider();
            $accessToken = $provider->getAccessToken('refresh_token', [
                'refresh_token' => $accessToken->getRefreshToken()
            ]);
            // Persist new token
            $this->config->accessToken = $accessToken->jsonSerialize();
        }

        return $accessToken;
    }

    public function doFreshLogin(OutputInterface $output)
    {
        $provider = $this->getProvider();

        $authorizationUrl = $provider->getAuthorizationUrl();
        $output->writeln(sprintf('Go to %s in your browser.', $authorizationUrl));
        // Generate a random state string.
        $this->oauth2state = $provider->getState();

        // This will cause $this->authCode to be set
        // Freeagent doesn't provide an option to display the token in the
        // browser, so we'll boot up a web server, and set the allowable
        // redirects to 127.0.0.1 in the freeagent integration settings.
        $this->waitForToken();

        try {
            // Try to get an access token using the authorization code grant.
            $accessToken = $provider->getAccessToken('authorization_code', [
                'code' => $this->authCode
            ]);

            // Persist access token
            $this->config->accessToken = $accessToken->jsonSerialize();

            // Using the access token, we may look up details about the
            // resource owner.
            $resourceOwner = $provider->getResourceOwner($accessToken);
            $resourceOwner = $resourceOwner->toArray();
            $output->writeln(sprintf(
                'Successfully authenticated: %s (%s). Token expiry: %s',
                $resourceOwner['company']['name'],
                $resourceOwner['company']['subdomain'],
                date('Y-m-d H:i:s', $accessToken->getExpires())
            ));
        } catch (\League\OAuth2\Client\Provider\Exception\IdentityProviderException $e) {
            // Failed to get the access token or user details.
            $output->writeln("Exception occurred: " . $e->getMessage());
            return false;
        }

        return true;
    }

    /**
     * Start an HTTP server and wait for a request with state and code to come
     * in. Set the code in the class and kill the HTTP server.
     */
    private function waitForToken()
    {
        $loop = \React\EventLoop\Factory::create();

        $server = new \React\Http\Server($loop, function (\Psr\Http\Message\ServerRequestInterface $request) use ($loop) {
            $queryParams = $request->getQueryParams();
            if (isset($queryParams['state'])) {
                if ($queryParams['state'] !== $this->oauth2state) {
                    echo "debug: oauth2state did not match";
                }
                $this->authCode = $queryParams['code'];
                // After receiving a request, shut down the event loop, which
                // will get us out of this function. Give it three secs just
                // incase one of them is an OPTIONS or something like that.
                $loop->addTimer(3.0, function() use ($loop) {
                    $loop->stop();
                });
                return new \React\Http\Message\Response(
                    200,
                    array(
                        'Content-Type' => 'text/plain'
                    ),
                    "You can go back to the console."
                );
            }
            return new \React\Http\Message\Response(
                200,
                array(
                    'Content-Type' => 'text/plain'
                ),
                print_r($request->getQueryParams(), true)
            );
        });

        $socket = new \React\Socket\Server(12423, $loop);
        $server->listen($socket);

        $loop->run();
    }
}