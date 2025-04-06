const express = require('express');
const router = express.Router();
const axios = require('axios');
const querystring = require('querystring');
const dataModel = require('../models/dataModel');
const path = require('path');

// Spotify credentials and redirect URI (update with your Render domain)
const clientID = process.env.CLIENT_ID;
const clientSecret = process.env.CLIENT_SECRET;
const redirectURI = "https://spotify-player-esp32.onrender.com/spotify/callback";

// Endpoint to redirect user to Spotify's OAuth page
router.get('/login', (req, res) => {
  const deviceId = req.query.id;

  if (!deviceId) {
    return res.status(400).send('Missing device ID');
  }

  const scope = 'user-read-playback-state user-read-currently-playing user-modify-playback-state';

  const authUrl = 'https://accounts.spotify.com/authorize?' + querystring.stringify({
    response_type: 'code',
    client_id: clientID,
    scope: scope,
    redirect_uri: redirectURI, 
    state: deviceId,
    show_dialog: true
  });
  
  res.redirect(authUrl);
});

// OAuth callback endpoint: Exchange code for tokens, broadcast them, then send success page
router.get('/callback', async (req, res) => {
  const code = req.query.code || null;
  const deviceId = req.query.state;

  if (!code || !deviceId) {
    return res.status(400).send('Missing authorization code or device ID.');
  }

  const tokenUrl = 'https://accounts.spotify.com/api/token';
  const data = {
    code,
    redirect_uri: redirectURI,
    grant_type: 'authorization_code'
  };

  const headers = {
    'Authorization': 'Basic ' + Buffer.from(`${clientID}:${clientSecret}`).toString('base64'),
    'Content-Type': 'application/x-www-form-urlencoded'
  };

  try {
    const response = await axios.post(tokenUrl, querystring.stringify(data), { headers });

    const refreshToken = response.data.refresh_token;
    const accessToken = response.data.access_token;

    console.log("Tokens generated:", { accessToken, refreshToken });

    // ✅ Save refresh token to DB
    await dataModel.updateRefreshToken({ id: deviceId, refresh_token: refreshToken });

    // Send success page
    res.sendFile(path.join(__dirname, '..', 'public', 'success.html'));
  } catch (error) {
    console.error("Error exchanging code for tokens:", error.response?.data || error.message);
    res.status(500).send('Error retrieving tokens.');
  }
});

module.exports = router;
