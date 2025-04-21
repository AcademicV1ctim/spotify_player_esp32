require('dotenv').config(); 

const { Pool } = require('pg');

// Connection settings for Neon (PostgreSQL)
const settings = {
  connectionString: process.env.DATABASE_URL, // Full connection string from Neon
  ssl: { rejectUnauthorized: false }, // Required for Neon SSL
  max: 25, // max number of clients in the pool
  idleTimeoutMillis: 30000, // close idle clients after 30s
};

const pool = new Pool(settings);

module.exports = pool;
