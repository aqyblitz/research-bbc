/* 
 * Build script
 */

/*global require,module*/

module.exports = function (grunt) {
    'use strict';
    // Load grunt tasks automatically
    require('load-grunt-tasks')(grunt);
  
    if(!grunt.file.exists(__dirname + '/run')){
       grunt.file.mkdir(__dirname + '/run');
    }

    // Define the configuration for all the tasks
    grunt.initConfig({
        config:   require('./config.json'),
        pconfig:  require('./problemConfig.json'),
        
        exec: { 
            client: {
                cwd: 'src', // add back http.cpp and move .resulting client .js file to www folder so it can be accessed.
                cmd: 'em++ client.cpp -O2 -o ../run/client.html -DINF=<%= pconfig.infinityVal %> -DPARAM=<%= pconfig.computeParam %> -DDEBUG=<%= config.debug %> -DSOCKK=<%= config.socketPort %> -DHOST_NAME="\\"<%= config.host %>\\"" -I include/ -std=c++11 --bind -s ALLOW_MEMORY_GROWTH=1' 
            },
            server: {
                cwd: 'src',
                cmd: 'em++ server.cpp -o ../run/server.js -DINF=<%= pconfig.infinityVal %> -DPARAM=<%= pconfig.computeParam %> -DPARAMA=<%= pconfig.paramA %> -DPARAMB=<%= pconfig.paramB %> -DDEBUG=<%= config.debug %> -DFILE_PATH="\\"<%= config.filePath %>\\"" -DSOCKK=<%= config.socketPort %> -DBLOCK_TOTAL=<%= pconfig.blockTotal %> -DRED_MULT=<%= pconfig.redMult %> -DVERTEX_TOTAL=<%= pconfig.vertexTotal %> -I include/ -std=c++11 -s ALLOW_MEMORY_GROWTH=1'
            }
        }
    });

    grunt.registerTask('compile', [
        'exec'
    ]);
 
    grunt.registerTask('compile-parallel', [
        'exec:server',
        'exec:client'
    ]);   
    grunt.registerTask('compile-client', [
        'exec:client'
    ]);
    grunt.registerTask('compile-server', [
        'exec:server'
    ]);

    grunt.registerTask('default', [
        'build'
    ]);
};
