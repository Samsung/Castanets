const path = require('path');

module.exports = function (config) {
  config.set({
    basePath: './',
    plugins: [
      'karma-webpack',
      'karma-mocha',
      'karma-expect',
      'karma-sinon',
      'karma-chrome-launcher',
      'karma-coverage'
    ],
    frameworks: ['mocha', 'expect', 'sinon'],
    files: ['offload.js/**/*.test.js'],
    exclude: [],
    preprocessors: {
      ['offload.js/**/*.test.js']: ['webpack']
    },
    webpack: {
      mode: 'development',
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
              {
                loader: 'style-loader',
                options: { injectType: 'lazyStyleTag' }
              },
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
          },
          {
            test: /\.js$/,
            enforce: 'pre',
            include: [path.resolve(__dirname, 'offload.js')],

            resolve: {
              alias: {
                '~': path.resolve(__dirname, 'offload.js')
              }
            },

            loader: 'istanbul-instrumenter-loader',
            options: {
              esModules: true
            }
          }
        ]
      }
    },
    reporters: ['progress', 'coverage'],
    coverageReporter: { type: 'text' },
    browsers: ['Chrome'],
    singleRun: true
  });
};
