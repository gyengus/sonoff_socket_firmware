var argv = global.argv = require('minimist')(process.argv.slice(2));
var gulp = require('gulp');
var runSequence = require('run-sequence');
var PluginError = require('plugin-error');
var exec = require('child_process').exec;
var fs = require('fs');
var rimraf = require('rimraf');
var mqtt = require('mqtt');

var buildConfig = JSON.parse(fs.readFileSync('./buildConfig.json', 'utf8'));

var device = argv.device;

gulp.task('clean', function(callback) {
	if (fs.existsSync(__dirname + '/' + buildConfig.arduino['build-path'])) {
		 rimraf(__dirname + '/' + buildConfig.arduino['build-path'] + '/*', callback);
	} else {
		fs.mkdirSync(__dirname + '/' + buildConfig.arduino['build-path']);
		callback();
	}
});

gulp.task('generate', function(callback) {
	if (device == undefined) {
		throw new PluginError('gulp', 'device is empty.', {showStack: false});
	}

	if (buildConfig.devices[device]) {
		var buf = "";
		buf = '#define DEVICE_NAME "' + device + '"\n\n'
			+ '#define MQTT_BROKER_ADDRESS "' + buildConfig.mqtt.host + '"\n'
			+ '#define MQTT_BROKER_PORT ' + buildConfig.mqtt.port + '\n'
			+ '#define MQTT_USER "' + buildConfig.devices[device].mqttUser + '"\n'
			+ '#define MQTT_PASSWORD "' + buildConfig.devices[device].mqttPassword + '"\n'
			+ '#define MQTT_TOPIC "' + buildConfig.devices[device].topic + '"\n'
			+ '#define MQTT_DEVICE_TOPIC "' + buildConfig.mqtt.deviceTopicPrefix + '"\n\n'
			+ 'const char *sta_ssid = "' + buildConfig.wifi.ssid + '";\n'
			+ 'const char *sta_password = "' + buildConfig.wifi.password + '";\n';
		fs.writeFileSync('./config.h', buf);
		callback();
	} else {
		throw new PluginError('generate', 'Device "' + device + '" is not found in buildConfig.json.', {showStack: false});
	}
});

gulp.task('build', ['clean'], function(callback) {
	if (device == undefined) {
		throw new PluginError('gulp', 'device is empty.', {showStack: false});
	}

	var command = buildConfig.arduino.root + buildConfig.arduino.cmd + ' -compile '
		    + '-hardware ' + buildConfig.arduino.root + buildConfig.arduino.hardware + ' '
		    + '-tools ' + buildConfig.arduino.root + buildConfig.arduino.tools + ' '
		    + '-libraries ' + buildConfig.arduino.root + buildConfig.arduino.libraries + ' '
		    + '-hardware ' + buildConfig.arduino.root + buildConfig.devices[device].hardware + ' '
		    + '-tools ' + buildConfig.arduino.root + buildConfig.devices[device].tools + ' '
		    + '-libraries ' + buildConfig.arduino.root + buildConfig.devices[device].libraries + ' '
		    + '-fqbn ' + buildConfig.devices[device].fqbn + ' ';
	if (buildConfig.devices[device].prefs != undefined) {
		for (var pref in buildConfig.devices[device].prefs) {
			command += '-prefs ' + buildConfig.devices[device].prefs[pref] + ' ';
		}
	}
	command += '-build-path ' + __dirname + '/' + buildConfig.arduino['build-path'] + ' '
			 + __dirname + '/' + buildConfig.arduino.src
	exec(command, function(err) {
		if (err) return callback(err);
		callback();
	});
});

gulp.task('update', function(callback) {
	if (device == undefined) {
		throw new PluginError('gulp', 'device is empty.', {showStack: false});
	}

	var mqttOptions = {
		'connectTimeout': 2000,
		'reconnectPeriod': 250,
		'keepalive': 0,
		'clientId': buildConfig.mqtt.clientId
	};
	if (buildConfig.mqtt.name) {
		mqttOptions.username = buildConfig.mqtt.name;
		if (buildConfig.mqtt.password) {
			mqttOptions.password = buildConfig.mqtt.password;
		}
	}
	var client = mqtt.connect('mqtt://' + buildConfig.mqtt.host + ':' + buildConfig.mqtt.port, mqttOptions);
	client.on('error', function(err) {
		throw new PluginError('mqtt', err, {showStack: false});
	});
	client.on('connect', function() {
		var firmware = fs.readFileSync(__dirname + '/' + buildConfig.arduino['build-path'] + '/' + buildConfig.arduino.src + '.bin');
		if (firmware) {
			client.publish(buildConfig.mqtt.deviceTopicPrefix + device + '/update',
				       firmware,
				       {
						'retain': true
				       },
				       function(err) {
							if (err) throw new PluginError('mqtt', err, {showStack: false});
							client.end();
							callback();
				       });
		} else {
			throw new PluginError('gulp', 'firmware not found.', {showStack: false});
		}
	});
});

gulp.task('default', function(callback) {
	runSequence('generate', 'build', 'update', callback);
});
