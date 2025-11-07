#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const { minify: minifyHTML } = require('html-minifier-terser');
const { minify: minifyJS } = require('terser');
const CleanCSS = require('clean-css');

const WEB_DIR = path.join(__dirname, 'web');
const OUTPUT_DIR = path.join(__dirname, 'data');

// Color output for terminal
const colors = {
  reset: '\x1b[0m',
  green: '\x1b[32m',
  blue: '\x1b[34m',
  yellow: '\x1b[33m',
  red: '\x1b[31m'
};

function log(message, color = 'reset') {
  console.log(`${colors[color]}${message}${colors.reset}`);
}

async function minifyCSS(css) {
  const result = new CleanCSS({
    level: 2,
    compatibility: '*'
  }).minify(css);

  if (result.errors.length > 0) {
    throw new Error(`CSS minification errors: ${result.errors.join(', ')}`);
  }

  return result.styles;
}

async function minifyJavaScript(js) {
  const result = await minifyJS(js, {
    compress: {
      dead_code: true,
      drop_console: false,
      drop_debugger: true,
      keep_fargs: false,
      passes: 2
    },
    mangle: {
      toplevel: false,  // Changed from true - don't mangle top-level names
      keep_fnames: true  // Preserve function names
    },
    format: {
      comments: false
    }
  });

  if (result.error) {
    throw new Error(`JS minification error: ${result.error}`);
  }

  return result.code;
}

async function minifyHTMLContent(html) {
  return await minifyHTML(html, {
    collapseWhitespace: true,
    removeComments: true,
    removeAttributeQuotes: true,
    minifyCSS: true,
    minifyJS: true,
    removeRedundantAttributes: true,
    useShortDoctype: true,
    removeEmptyAttributes: true
  });
}

async function processFile(filePath, minifier, ext) {
  const fileName = path.basename(filePath, ext);
  const outputPath = path.join(OUTPUT_DIR, `${fileName}.min${ext}`);

  log(`📄 Processing ${path.basename(filePath)}...`, 'blue');

  const content = fs.readFileSync(filePath, 'utf-8');
  const originalSize = content.length;

  try {
    const minified = await minifier(content);
    fs.writeFileSync(outputPath, minified, 'utf-8');

    const newSize = minified.length;
    const saved = originalSize - newSize;
    const percent = ((saved / originalSize) * 100).toFixed(1);

    log(`  ✓ ${fileName}.min${ext} created`, 'green');
    log(`  📊 ${originalSize} → ${newSize} bytes (saved ${saved} bytes, ${percent}%)`, 'yellow');

    return { originalSize, newSize, saved };
  } catch (error) {
    log(`  ✗ Error: ${error.message}`, 'red');
    throw error;
  }
}

async function main() {
  log('\n🔧 Starting web asset minification...', 'blue');
  log(`📁 Source directory: ${WEB_DIR}\n`, 'blue');

  // Check if web directory exists
  if (!fs.existsSync(WEB_DIR)) {
    log(`✗ Error: ${WEB_DIR} directory not found!`, 'red');
    log(`Creating web directory...`, 'yellow');
    fs.mkdirSync(WEB_DIR, { recursive: true });
  }

  const stats = {
    totalOriginal: 0,
    totalMinified: 0,
    totalSaved: 0
  };

  // Find all HTML, CSS, and JS files
  const files = fs.readdirSync(WEB_DIR);

  for (const file of files) {
    const filePath = path.join(WEB_DIR, file);

    // Skip if it's already a minified file or not a file
    if (file.includes('.min.') || !fs.statSync(filePath).isFile()) {
      continue;
    }

    try {
      let result;

      if (file.endsWith('.html')) {
        result = await processFile(filePath, minifyHTMLContent, '.html');
      } else if (file.endsWith('.css')) {
        result = await processFile(filePath, minifyCSS, '.css');
      } else if (file.endsWith('.js')) {
        result = await processFile(filePath, minifyJavaScript, '.js');
      } else {
        continue;
      }

      stats.totalOriginal += result.originalSize;
      stats.totalMinified += result.newSize;
      stats.totalSaved += result.saved;

      log('');
    } catch (error) {
      log(`✗ Failed to process ${file}`, 'red');
    }
  }

  // Summary
  const totalPercent = stats.totalOriginal > 0 
    ? ((stats.totalSaved / stats.totalOriginal) * 100).toFixed(1)
    : 0;

  log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━', 'blue');
  log('📊 Minification Summary:', 'blue');
  log(`  Total original: ${stats.totalOriginal.toLocaleString()} bytes`);
  log(`  Total minified: ${stats.totalMinified.toLocaleString()} bytes`);
  log(`  Total saved: ${stats.totalSaved.toLocaleString()} bytes (${totalPercent}%)`, 'green');
  log('✅ Minification complete!\n', 'green');
}

// Main execution
(async () => {
  try {
    await main();
    process.exit(0);
  } catch (error) {
    log(`\n✗ Fatal error: ${error.message}`, 'red');
    console.error(error.stack);
    process.exit(1);
  }
})();
