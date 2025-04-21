const pool = require('../services/db.js');

// Check if refresh token exists for a user
module.exports.checkRefreshToken = async (data, callback) => {
    const SQLSTATEMENT = `
        SELECT refresh_token 
        FROM "Users" 
        WHERE id = $1;
    `;
    const VALUES = [data.id];

    try {
        const result = await pool.query(SQLSTATEMENT, VALUES);
        callback(null, result);
    } catch (error) {
        callback(error);
    }
};