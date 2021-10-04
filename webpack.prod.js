const { merge } = require('webpack-merge');
const TerserPlugin = require('terser-webpack-plugin');
const FileManagerPlugin = require('filemanager-webpack-plugin');
const common = require('./webpack.common.js');

module.exports = merge(common, {
  mode: 'production',
  optimization: {
    minimizer: [
      new TerserPlugin({
        extractComments: false,
        terserOptions: {
          compress: {
            pure_funcs: ['console.debug', 'console.time', 'console.timeEnd']
          }
        }
      })
    ]
  },
  plugins: [
    new FileManagerPlugin({
      onStart: {
        delete: ['./dist/']
      }
    })
  ]
});
