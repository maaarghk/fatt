<?php

namespace App;

class Config
{
    public $environment = "sandbox";
    public $clientId = "";
    public $clientSecret = "";
    public $accessToken;
    public $timeslipUser;

    public function load($filename)
    {
        @$text = file_get_contents($filename);
        @$data = json_decode($text, true);
        @$this->environment = $data['environment'];
        @$this->clientId = $data['clientId'];
        @$this->clientSecret = $data['clientSecret'];
        @$this->accessToken = $data['accessToken'];
        @$this->timeslipUser = $data['timeslipUser'];
    }

    public function save($filename)
    {
        if (!file_exists($filename)) {
            touch($filename);
        }
        $array = [
            "environment" => $this->environment,
            "clientId" => $this->clientId,
            "clientSecret" => $this->clientSecret,
            "accessToken" => $this->accessToken,
            "timeslipUser" => $this->timeslipUser,
        ];
        file_put_contents($filename, json_encode($array));
    }
}