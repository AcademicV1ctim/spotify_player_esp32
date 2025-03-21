require('dotenv').config();
const express = require('express');
const axios = require('axios');
const querystring = require('querystring');
const app = express();
const port = process.env.PORT || 3000;

// Replace these with your actual Spotify app credentials and redirect URI.
const clientID = process.env.CLIENT_ID;
const clientSecret = process.env.CLIENT_SECRET;
const redirectURI = "http://localhost:3000/callback";

// Endpoint to redirect user to Spotify's OAuth page
app.get('/login', (req, res) => {
  const scope = 'user-read-playback-state user-read-currently-playing user-modify-playback-state';
  const authUrl = 'https://accounts.spotify.com/authorize?' +
    querystring.stringify({
      response_type: 'code',
      client_id: clientID,
      scope: scope,
      redirect_uri: redirectURI
    });
  res.redirect(authUrl);
});

// OAuth callback endpoint
app.get('/callback', async (req, res) => {
  const code = req.query.code || null;
  if (code === null) {
    return res.status(400).send('Error: No authorization code provided.');
  }

  const tokenUrl = 'https://accounts.spotify.com/api/token';
  const data = {
    code: code,
    redirect_uri: redirectURI,
    grant_type: 'authorization_code'
  };

  const headers = {
    'Authorization': 'Basic ' + Buffer.from(clientID + ':' + clientSecret).toString('base64'),
    'Content-Type': 'application/x-www-form-urlencoded'
  };

  try {
    const response = await axios.post(tokenUrl, querystring.stringify(data), { headers: headers });
    const access_token = response.data.access_token;
    const refresh_token = response.data.refresh_token;

    // For now, just display the tokens; in a real application, you might store them or use them to make API calls.
    res.send(`
      <h2>Authentication Successful!</h2>
      <p><strong>Access Token:</strong> ${access_token}</p>
      <p><strong>Refresh Token:</strong> ${refresh_token}</p>
    `);
  } catch (error) {
    console.error("Error exchanging code for tokens:", error);
    res.status(500).send('Error retrieving tokens.');
  }
});

app.listen(port, () => {
  console.log(`Server is running on port ${port}`);
});
