# How to use Apache as front-end

## Introduction

Since Apache does not by default support web sockets, it is not the most
straightforward approach for a ParaViewWeb deployment. However, many users
require or employ such a front-end, so this document provides a step-by-step
guide on how to setup such an environment.

## Components overview

A web application that relies on ParaViewWeb is composed of two to three
components, depending on the type of deployment and desired usage.

The two main components are:

1. The web socket server that is used to control the ParaView engine.
2. Some static content that contains the JavaScript and web pages. This content
composes the web application that needs to be delivered via standard HTTP.

In such a setup, the ParaView process acts as the web server and only a shared
visualization can be achieved. In order to support multiple clients and
therefore concurrent visualizations, a third component is needed that will act
as a session manager by starting and stoping the ParaView sessions and
communication forwarder for the web socket.

The following image illustrates what a multi-user deployment could look like
behind an Apache front-end, with a Java web server handling the ParaView processes.

{@img images/ParaViewWeb-Apache.png Alt text}

## Installation

### ParaView

ParaView needs to be built from source in order to support the latest ParaViewWeb
features. This section will assume a Unix-based environment and will go over the
command lines needed for configuration, build, and installation of the ParaView
components for a ParaViewWeb deployment.

    $ cd ParaViewWeb
    $ mkdir ParaView
    $ cd ParaView
    $ mkdir build install
    $ git clone git://paraview.org/stage/ParaView.git src
    $ cd src
    $ git checkout -b ParaViewWeb2.0 origin/ParaViewWeb2.0
    $ git submodule update --init
    $ cd ../build
    $ ccmake ../src
    
        PARAVIEW_ENABLE_PYTHON      ON
        PARAVIEW_BUILD_QT_GUI       OFF
        Module_vtkParaViewWeb       ON
        CMAKE_INSTALL_PREFIX        /.../ParaView/install
    
    $ make
    $ make install

### ParaViewWeb

In this example, ParaViewWeb is composed of Python scripts (web server),
a JavaScript library, and a set of sample web applications. In order to install
it, download the following [archive](guides/apache_setup/data/ParaViewWeb.tgz).

    $ cd ParaViewWeb 
    $ mkdir web-server
    $ cd web-server
    $ curl http://pvw.kitware.com/guides/apache_setup/data/ParaViewWeb.tgz -O
    $ tar xvzf ParaViewWeb.tgz

After unarchiving the file, there will be two main directories, one for the pvpython
modules (server) and one for the static web content (web-content). The static web
content directory will need to be served by a session manager like the one below.

### Session Manager

ParaViewWeb comes with several external projects to support various types of deployment.
One ParaViewWeb deployment involves a web front-end that accommodates concurrent
ParaViewWeb visualization sessions.
This session manager uses the Jetty libraries to provide a fully functional
web server inside an embedded application. More information can be found on the
[Jetty Sessions Manager](index.html#!/guide/jetty_session_manager) and developer
team, and the executable can be downloaded
[here](http://pvw.kitware.com/SessionManager/project-summary.html).

The executable can be downloaded [here](guides/jetty_session_manager/data/JettySessionManager-Server-1.0.jar)

Configuration file for the session manager executable: (pw-config.properties):

    # Web setup
    pw.web.port=9000
    pw.web.content.dir=
    
    # Process logs
    pw.logging.dir=/tmp/pw-logs
    
    # Process commands
    pw.default.cmd=
    pw.default.cmd.run.dir=/tmp/
    pw.default.cmd.map=PORT:getPort|HOST:getHost
    
    # Resources informations
    pw.resources=localhost:9001-9100
    
    # Factory
    pw.factory.proxy.adapter=com.kitware.paraviewweb.external.JsonFileProxyConnectionAdapter
    pw.factory.wamp.url.generator=com.kitware.paraviewweb.external.GenericWampURLGenerator
    pw.factory.resource.manager=com.kitware.paraviewweb.external.SimpleResourceManager
    pw.factory.visualization.launcher=com.kitware.paraviewweb.external.ProcessLauncher
    pw.factory.websocket.proxy=com.kitware.paraviewweb.external.SimpleWebSocketProxyManager
    pw.factory.session.manager=com.kitware.paraviewweb.external.MemorySessionManager
    
    # External configurations
    pw.factory.proxy.adapter.file=/home/pvw/proxy/session.map
    pw.factory.wamp.url.generator.pattern=ws://localhost/proxy?sessionId=SESSION_ID
    
    pw.process.launcher.wait.keyword=Starting factory
    pw.process.launcher.wait.timeout=10000

    pw.session.public.fields=id,sessionURL,name,description,url,application,idleTimeout,startTime

Shell script used to start the session manager

    export DISPLAY=:0.0
    java -jar JettySessionManager-Server-1.0.jar -Dpw-config=/.../pw-config.properties

### Python 2.7 ###

The websocket proxy requires Python 2.7 with the following packages installed:

#### 1. AutobahnPython ####

Download the [zipped file](http://pypi.python.org/pypi/autobahn).

    $ unzip autobahn-0.5.9.zip
    $ cd autobahn-0.5.9
    $ python setup.py build
    $ sudo python setup.py install

#### 2. pywebsockets ####

Download the [tarball](http://code.google.com/p/pywebsocket/downloads).

    $ tar xfz mod_pywebsocket-0.7.8.tar.gz 
    $ cd pywebsocket-0.7.8/src/
    $ python setup.py build
    $ sudo python setup.py install

### Apache ###

To proxy websocket traffic through Apache, a custom proxy is required. The proxy
can be download [here](guides/apache_setup/data/ApacheWebsocketProxy.tgz).

    $ tar xfz ApacheWebsocketProxy.tgz

The following modules need to be installed and configured to enable the proxy code.

#### mod_python ####

mod_pywebsocket is used by the websocket proxy to accept incoming websocket
connections, it requires mod_python.

    $ sudo apt-get install libapache2-mod-python

#### mod_pywebsocket ####

To enable the mod_pywebsocket module, add the following lines to /etc/apache2/apache2.conf

    PythonPath "sys.path+['/usr/local/lib/python2.7/dist-packages/mod_pywebsocket']"
    PythonOption mod_pywebsocket.handler_root <path_to_apache_websocket_proxy_code>
    PythonHeaderParserHandler mod_pywebsocket.headerparserhandler

#### Configuring the proxy ####

The following entries need to be added to the /etc/apache2/sites-available/pvw configuration file.

    AddHandler mod_python .py
    PythonHandler mod_python.publisher

The configuration for the proxy is held in proxy.json, that looks like this:

    {
      "loggingConfiguration": "/home/pvw/proxy/logging.json",
      "connectionReaper": {
          "reapInterval": 300,
          "connectionTimeout": 3600
      },
      "sessionMappingFile": "/home/pvw/proxy/session.map"
    }

* *loggingConfigurations* - This is the path to the JSON file containing the Python logging configuration.
* *connectionReaper.reapInterval* - This is the interval at which the connection reaper is run at in seconds.
* *connectionReaper.connectionTimeout* - This is the length of time a connection can remain inactive before the connection will be cleaned up.
* *sessionMappingFile* - This is the session mapping file produced by the session manager. It maps session IDs to connection endpoints,
    the proxy uses this to know where to route sessions. See Session manager configuration for details 

Place the proxy.json configuration file in the home directory of the user used
to run apache, usally /var/www

#### Virtual host configuring ####

The virtual host should be the following:

    <VirtualHost *:80>
        ServerName  your.paraviewweb.hostname
        ServerAdmin your@email.com
    
        # Static content directory (js, html, png, css)
        DocumentRoot /home/pvw/ParaViewWeb/www
        
        # Jetty Session Manager
        ProxyPass        /paraview  http://localhost:9000/paraview
        ProxyPassReverse /paraview  http://localhost:9000/paraview
    
        # Proxy web socket to ParaViewWeb processes
        <Directory /home/pvw/ParaViewWeb/www/websockets>
                Options Indexes FollowSymLinks MultiViews
                AllowOverride None
                Order allow,deny
                allow from all
                AddHandler mod_python .py
                PythonHandler mod_python.publisher
                PythonDebug On
        </Directory>
     
        # Log management
        LogLevel warn
        ErrorLog  /home/pvw/ParaViewWeb/logs/pvw-error.log`
        CustomLog /home/pvw/ParaViewWeb/logs/pvw-access.log combined
    </VirtualHost>
