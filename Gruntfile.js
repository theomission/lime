'use strict';

var jade = require('jade');

module.exports = function(grunt) {
  grunt.initConfig({
    pkg: grunt.file.readJSON('package.json'),
    watch: {
      options: {
        livereload: true
      },
      jade: {
        tasks: ["jade:compile"],
        files: ["**/*.jade", "!layout/*.jade", "!node_modules/**/*.jade"]
      }
    },
    jade: {
      compile: {
        options: {
          prettye: true
        },
        files: {
          "./index.html" : "./src/index.jade"
        }
      }
    }
  });

  grunt.loadNpmTasks('grunt-contrib-watch');
  grunt.loadNpmTasks('grunt-contrib-jade');

  grunt.registerTask('default', ['jade:compile']);
};
