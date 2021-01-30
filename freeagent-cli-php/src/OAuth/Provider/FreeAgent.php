<?php
/**
 * Freeagent provider for league/ouath2-client
 * You can pass bool sandbox in the options array to change base URL, then you
 * can make getAuthenticatedRequests calls with a relative URL and the base will
 * be prepended.
 */

namespace App\OAuth\Provider;

use League\OAuth2\Client\Provider\AbstractProvider;
use League\OAuth2\Client\Provider\Exception\IdentityProviderException;
use League\OAuth2\Client\Provider\GenericResourceOwner;
use League\OAuth2\Client\Token\AccessToken;
use League\OAuth2\Client\Tool\BearerAuthorizationTrait;
use Psr\Http\Message\ResponseInterface;

class FreeAgent extends AbstractProvider
{
    use BearerAuthorizationTrait;

    /**
     * @var string
     */
    private $responseError = 'error';

    /**
     * @var string
     */
    private $responseCode;

    /**
     * @var string
     */
    private $responseResourceOwnerId = 'id';

    protected $baseUrl = 'https://api.freeagent.com/v2/';

    public function __construct(array $options = array())
    {
        parent::__construct($options);
        if (isset($options['sandbox']) && $options['sandbox']) {
            $this->baseUrl = 'https://api.sandbox.freeagent.com/v2/';
        }
    }

    public function getBaseAuthorizationUrl()
    {
        return $this->baseUrl . 'approve_app';
    }

    public function getBaseAccessTokenUrl($params)
    {
        return $this->baseUrl . 'token_endpoint';
    }

    public function getResourceOwnerDetailsUrl(AccessToken $token = null)
    {
        return $this->baseUrl . 'company';
    }

    protected function getDefaultScopes()
    {
        return null;
    }

    // From GenericProvider
    protected function checkResponse(ResponseInterface $response, $data)
    {
        if (!empty($data[$this->responseError])) {
            $error = $data[$this->responseError];
            if (!is_string($error)) {
                $error = var_export($error, true);
            }
            $code  = $this->responseCode && !empty($data[$this->responseCode])? $data[$this->responseCode] : 0;
            if (!is_int($code)) {
                $code = intval($code);
            }
            throw new IdentityProviderException($error, $code, $data);
        }
    }

    /**
     * @inheritdoc
     */
    protected function createResourceOwner(array $response, AccessToken $token)
    {
        return new GenericResourceOwner($response, $this->responseResourceOwnerId);
    }

    /**
     * @inheritDoc
     */
    public function getAuthenticatedRequest($method, $url, $token, array $options = [])
    {
        if (strpos($url, 'http:') === false && strpos($url, 'https:') === false) {
            $url = $this->baseUrl . $url;
        }
        return $this->createRequest($method, $url, $token, $options);
    }
}