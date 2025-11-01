const { src, dest, series, parallel } = require('gulp');
const concat = require('gulp-concat');
const gzip = require('gulp-gzip');
const postcss = require('gulp-postcss');
const cssnano = require('cssnano');
const terser = require('gulp-terser');
const jsonminify = require('gulp-jsonminify');
const htmlmin = require('gulp-html-minifier-terser');
const replace = require('gulp-replace');

// Paths for tasks
let paths = {
  styles: {
    dest: 'data/static/',
    bundles: {
      'app.css': [
        'src_data/styles/pico.min.css',
        'src_data/styles/iconly.css',
        'src_data/styles/app.css'
      ]
    }
  },
  scripts: {
    dest: 'data/static/',
    bundles: {
      'app.js': [
        'src_data/scripts/i18n.min.js',
        'src_data/scripts/lang.js',
        'src_data/scripts/utils.js'
      ],
      'chart.js': [
        'src_data/scripts/chart.js'
      ]
    }
  },
  json: [
    {
      src: 'src_data/locales/*.json',
      dest: 'data/static/locales/'
    },
    {
      src: 'src_data/*.json',
      dest: 'data/static/'
    }
  ],
  static: [
    {
      src: 'src_data/fonts/*.*',
      dest: 'data/static/fonts/'
    },
    {
      src: 'src_data/images/*.*',
      dest: 'data/static/images/'
    },
    {
      src: 'src_data/*.txt',
      dest: 'data/static/'
    }
  ],
  pages: {
    src: 'src_data/pages/*.html',
    dest: 'data/pages/'
  }
};



// Tasks
const styles = (cb) => {
  for (let name in paths.styles.bundles) {
    const items = paths.styles.bundles[name];

    src(items)
      .pipe(replace(
        "{BUILD_TIME}",
        Math.floor(Date.now() / 1000)
      ))
      .pipe(postcss([
        cssnano({ preset: 'advanced' })
      ]))
      .pipe(concat(name))
      .pipe(gzip({
        append: true
      }))
      .pipe(dest(paths.styles.dest));
  }

  cb();
}

const scripts = (cb) => {
  for (let name in paths.scripts.bundles) {
    const items = paths.scripts.bundles[name];

    src(items)
      .pipe(replace(
        "{BUILD_TIME}",
        Math.floor(Date.now() / 1000)
      ))
      .pipe(terser().on('error', console.error))
      .pipe(concat(name))
      .pipe(gzip({
        append: true
      }))
      .pipe(dest(paths.scripts.dest));
  }

  cb();
}

const jsonFiles = (cb) => {
  for (let i in paths.json) {
    const item = paths.json[i];

    src(item.src)
      .pipe(replace(
        "{BUILD_TIME}",
        Math.floor(Date.now() / 1000)
      ))
      .pipe(jsonminify())
      .pipe(gzip({
        append: true
      }))
      .pipe(dest(item.dest));
  }

  cb();
}

const staticFiles = (cb) => {
  for (let i in paths.static) {
    const item = paths.static[i];

    src(item.src, { encoding: false })
      .pipe(gzip({
        append: true
      }))
      .pipe(dest(item.dest));
  }

  cb();
}

const pages = () => {
  return src(paths.pages.src)
    .pipe(replace(
      "{BUILD_TIME}",
      Math.floor(Date.now() / 1000)
    ))
    .pipe(htmlmin({
      html5: true,
      caseSensitive: true,
      collapseWhitespace: true,
      collapseInlineTagWhitespace: true,
      conservativeCollapse: true,
      removeComments: true,
      minifyJS: true
    }))
    .pipe(gzip({
      append: true
    }))
    .pipe(dest(paths.pages.dest));
}

exports.build_styles = styles;
exports.build_scripts = scripts;
exports.build_json = jsonFiles;
exports.build_static = staticFiles;
exports.build_pages = pages;
exports.build_all = parallel(styles, scripts, jsonFiles, staticFiles, pages);