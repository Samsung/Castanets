const path = require('path');
const FileManagerPlugin = require('filemanager-webpack-plugin');

module.exports = {
  name: 'offload',
  node: {
    fs: 'empty'
  },
  entry: {
    offload: [path.resolve(__dirname, './offload.js/offload/index.js')],
    'offload-worker': [
      path.resolve(__dirname, './offload.js/offload-worker/index.js')
    ]
  },
  output: {
    path: path.join(__dirname, './dist'),
    filename: '[name].js',
    library: '[name]',
    libraryTarget: 'window',
    libraryExport: 'default'
  },
  resolve: {
    alias: {
      '~': path.resolve(__dirname, './offload.js')
    }
  },
  module: {
    rules: [
      {
        test: /\.js$/,
        exclude: /(node_modules)|(dist)/,
        use: [
          {
            loader: 'babel-loader',
            options: {
              presets: [
                [
                  '@babel/preset-env',
                  {
                    exclude: ['transform-regenerator']
                  }
                ]
              ],
              plugins: [
                [
                  '@babel/plugin-transform-modules-commonjs',
                  {
                    allowTopLevelThis: true
                  }
                ]
              ]
            }
          }
        ]
      },
      {
        test: /\.css$/,
        use: [
          { loader: 'style-loader', options: { injectType: 'lazyStyleTag' } },
          'css-loader'
        ]
      },
      {
        test: /\.(png|jpg)$/,
        use: ['url-loader']
      },
      {
        test: /\.html$/,
        use: ['mustache-loader']
      }
    ]
  },
  plugins: [
    new FileManagerPlugin({
      onEnd: {
        copy: [
          // Tizen Offload Server
          {
            source: './dist/offload.js',
            destination: './offload-server/tizen/service/gen/public/'
          },
          {
            source: './dist/offload-worker.js',
            destination: './offload-server/tizen/service/gen/public/'
          },
          {
            source: './offload-server/src/public/**',
            destination: './offload-server/tizen/service/gen/public/'
          },
          {
            source: './offload-worker/src/index.html',
            destination:
              './offload-server/tizen/service/gen/public/offload-worker.html'
          },
          {
            source: './offload-server/src/**',
            destination: './offload-server/tizen/service/gen/'
          },
          // Tizen Offload Worker
          {
            source: './dist/offload-worker.js',
            destination: './offload-worker/tizen/gen/'
          },
          // Android Offload Worker
          {
            source: './dist/offload-worker.js',
            destination: './offload-worker/android/app/src/main/assets/'
          }
        ]
      }
    })
  ]
};
