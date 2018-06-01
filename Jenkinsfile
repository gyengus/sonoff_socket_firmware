devices = ['esp8266-sonoff-socket-1', 'esp8266-sonoff-socket-2'];

pipeline {
  agent any
  environment {
    BUILDCONFIG_PATH = credentials('buildConfig-sonoff-socket-s20')
  }
  stages {
    stage('npm_install') {
      steps {
	echo 'Install requirements...'
        sh 'npm install'
      }
    }
    stage('build') {
      steps {
	echo 'Buildconfig path: ' + $BUILDCONFIG_PATH
	build_all(devices)
      }
    }
  }
}

@NonCPS
def build_all(list) {
	list.each { item ->
		echo 'Building device: ${item}'
		sh 'gulp --device=${item}'
	}
}
