const { src, dest, parallel } = require('gulp');
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
      'settings-converter.js': [
        'src_data/scripts/settings-converter.js'
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
const styles = parallel(...Object.entries(paths.styles.bundles).map(([name, items]) => {
  return () => src(items)
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
}));

const scripts = parallel(...Object.entries(paths.scripts.bundles).map(([name, items]) => {
  return () => src(items)
    .pipe(replace(
      "{BUILD_TIME}",
      Math.floor(Date.now() / 1000)
    ))
    .pipe(terser())
    .pipe(concat(name))
    .pipe(gzip({
      append: true
    }))
    .pipe(dest(paths.scripts.dest));
}));

const jsonFiles = parallel(...paths.json.map((item) => {
  return () => src(item.src)
    .pipe(replace(
      "{BUILD_TIME}",
      Math.floor(Date.now() / 1000)
    ))
    .pipe(jsonminify())
    .pipe(gzip({
      append: true
    }))
    .pipe(dest(item.dest));
}));

const staticFiles = parallel(...paths.static.map((item) => {
  return () => src(item.src, { encoding: false })
    .pipe(gzip({
      append: true
    }))
    .pipe(dest(item.dest));
}));

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
