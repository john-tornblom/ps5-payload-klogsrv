# PS5 klog server
ps5-payload-klogsrv is a simple socket server for jailbroken PS5s that redirects
/dev/klog to sockets connected on port 3232.

## Building
Assuming you have the [ps5-payload-sdk][sdk] installed on a POSIX machine,
ps5-payload-klogsrv can be compiled using the following two commands:
```console
john@localhost:ps5-payload-klogsrv$ export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
john@localhost:ps5-payload-klogsrv$ make
```

## Depolyment
To deploy the payload to your PS5, you can use the 'test' make target as follows:
```console
john@localhost:ps5-payload-klogsrv$ make test
```

## Usage
To view the raw output of /dev/klog, we can connect to the socket server using
netcat:
```console
john@localhost:ps5-payload-klogsrv$ export PS5_HOST=ps5
john@localhost:ps5-payload-klogsrv$ nc $PS5_HOST 3232
```
If you like to format or filter the output, have a look at the script: `klogfmt.py`.
For example:
```console
john@localhost:ps5-payload-klogsrv$ export PS5_HOST=ps5
john@localhost:ps5-payload-klogsrv$ nc $PS5_HOST 3232 | python klogfmt.py
```

## Reporting Bugs
If you encounter problems with ps5-payload-klogsrv, please [file a github issue][issues].
If you plan on sending pull requests which affect more than a few lines of code,
please file an issue before you start to work on you changes. This will allow us
to discuss the solution properly before you commit time and effort.

## License
ps5-payload-klogsrv is licensed under the GPLv3+.
[sdk]: https://github.com/john-tornblom/ps5-payload-sdk
[issues]: https://github.com/john-tornblom/ps5-payload-klogsrvissues/new
