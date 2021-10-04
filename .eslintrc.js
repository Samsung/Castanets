module.exports = {
  extends: ['google', 'prettier'],
  parser: 'babel-eslint',
  parserOptions: {
    sourceType: 'module',
    ecmaVersion: 6
  },
  rules: {
    'guard-for-in': 1,
    'require-jsdoc': 0,
    'valid-jsdoc': 0,
    'new-cap': 1,
    'no-unused-vars': 1,
    'prefer-spread': 1,
    'no-new-object': 1,
    'arrow-parens': ['error', 'as-needed'],
    eqeqeq: ['error', 'always'],
    curly: ['error', 'all']
  },
  ignorePatterns: [
    '.circleci/**/*.js',
    '.gradle/**/*.js',
    'camera-api/**/*.js',
    'dist/**/*.js',
    'offload-server/tizen/**/*.js',
    'offload-worker/android/**/*.js',
    'offload-worker/tizen/**/*.js',
    'offload.js/offload/task/serialize-javascript.js',
    'sample/**/*.js',
    'service/**/*.js',
    'static-analyzer/**/*.js',
    'test/tizen/**/*.js'
  ]
};
