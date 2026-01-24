const fs = require('fs');
const { spawnSync } = require('child_process');
const path = require('path');

const package_json_path = path.join(__dirname, 'package.json');
const original_package_json = fs.readFileSync(package_json_path, 'utf8');
const pkg = JSON.parse(original_package_json);

function run_command(command, args) {
    const full_command = `${command} ${args.join(' ')}`;
    console.log(`> ${full_command}`);
    // When shell: true, passing command + args as a string avoids DEP0190
    const result = spawnSync(full_command, { stdio: 'inherit', shell: true });
    return result.status;
}

function restore_package_json() {
    fs.writeFileSync(package_json_path, original_package_json);
}

// Ensure cleanup on exit
process.on('SIGINT', restore_package_json);
process.on('exit', restore_package_json);

try {
    // 1. Run CJS Tests
    console.log('\n========================================');
    console.log('Running CJS Tests...');
    console.log('========================================\n');
    
    // Ensure type is commonjs
    pkg.type = 'commonjs';
    fs.writeFileSync(package_json_path, JSON.stringify(pkg, null, 2));

    const cjs_result = run_command('npm', ['run', 'test:cjs']);
    if (cjs_result !== 0) {
        console.error('CJS Tests Failed!');
        process.exit(cjs_result);
    }

    // 2. Run ESM Tests
    console.log('\n========================================');
    console.log('Running ESM Tests...');
    console.log('========================================\n');

    // Switch type to module
    pkg.type = 'module';
    fs.writeFileSync(package_json_path, JSON.stringify(pkg, null, 2));

    const esm_result = run_command('npm', ['run', 'test:esm']);
    if (esm_result !== 0) {
        console.error('ESM Tests Failed!');
        process.exit(esm_result);
    }

    console.log('\n========================================');
    console.log('ALL TESTS PASSED');
    console.log('========================================\n');

} catch (err) {
    console.error('Test execution error:', err);
    process.exit(1);
} finally {
    restore_package_json();
}
