// routes/dataRoutes.js
const express = require('express');
const { v5: uuidv5 } = require('uuid');
const router = express.Router();
const dataModel = require('../models/dataModel');
const { error } = require('console');

const NAMESPACE = process.env.UUIDv4; 

router.post('/register', async (req, res) => {
  const { mac } = req.body;

  if (!mac) {
    return res.status(400).send('Missing MAC address.');
  }

  try {
    // Generate UUIDv5 using the MAC address as the name
    const deviceId = uuidv5(mac, NAMESPACE);
    console.log('Generated UUID:', deviceId);

    // Store in Neon (Postgres) via dataModel
    await dataModel.registerUser(deviceId);

    if (!response.ok) {
        return res.status(400).json({message: `${errorMessage}`})
    }

    // Redirect or send response
    res.redirect('/login'); // or wherever your flow goes next
  } catch (err) {
    console.error('Error registering device:', err);
    res.status(500).send('Server error.');
  }
});

// Retrieve refresh token by device UUID
router.get('/retrieve', async (req, res) => {
    const { id } = req.query; // Use query for GET requests
  
    if (!id) {
      return res.status(400).json({ error: 'Missing id parameter' });
    }
  
    dataModel.checkRefreshToken({ id }, (err, result) => {
      if (err) {
        console.error('Database error:', err);
        return res.status(500).json({ error: 'Database error' });
      }
  
      if (result.rows.length === 0 || !result.rows[0].refresh_token) {
        return res.status(404).json({ error: 'Refresh token not found' });
      }
  
      res.status(200).json({ refresh_token: result.rows[0].refresh_token });
    });
});  

module.exports = router;
