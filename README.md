# libvmod-querymodifier

This is a simple Varnish VMOD that allows modification of a URL's query parameters by including or excluding specified parameters and their values.

## Status

ℹ️ This VMOD is currently exploratory and being actively developed and tested. It has not been run in any production environment yet,
so you should not yet use this in a production environment. 

If you need to manipulate querystrings in production, you should currently explore [`libvmod-queryfilter`](https://github.com/nytimes/libvmod-queryfilter/) or [`vmod-querystring`](https://git.sr.ht/~dridi/vmod-querystring).

If instead you just want to contribute to a friendly VMOD repository, continue on!

## Usage

### Inclusion

List the parameters that you would like to have _remain_ in the URL. All other query parameters and their values will be removed.

```
import querymodifier;
set req.url = querymodifier.modifyparams(url=req.url, params="search,id", exclude_params=false);

# Original URL: example.com/?search=name&ts=123456789&id=987654321
# Modified URL: example.com/?search=name&id=987654321
```

### Exclusion

List the parameters that you would like to have _removed_ from the URL. All other query parameters and their values will remain.

```
import querymodifier;
set req.url = querymodifier.modifyparams(url=req.url, params="ts,v", exclude_params=true);

# Original URL: example.com/?search=name&ts=123456789&v=123456789&id=987654321
# Modified URL: example.com/?search=name&id=987654321
```

### Remove all

Remove all query parameters by passing in an empty string.

```
import querymodifier;
set req.url = querymodifier.modifyparams(url=req.url, params="", exclude_params=true);

# Original URL: example.com/?search=name&ts=123456789&v=123456789&id=987654321
# Modified URL: example.com/
```

### Additional

See the tests for more parameter edge cases.

## Building

This module is primarily manually tested with Varnish 7.6, but also includes vtc tests for version 7.5.

```
./bootstrap
make
make check # optionally run tests, recommended.
sudo make install
```

## Contributing

Fork, code, and PR! See build instructions above.

I'm happy to review any PRs. Any bug reports are also welcome.

## Acknowledgements

- The NY Times [`libvmod-queryfilter` VMOD](https://github.com/nytimes/libvmod-queryfilter/) for insipiration.
- [`vcdk`](https://github.com/nigoroll/vcdk/) for the project structure.
- Guillaume Quintard for the [VMOD tutorial](https://info.varnish-software.com/blog/creating-a-vmod-vmod-str).

## License

[Apache 2.0](https://www.apache.org/licenses/LICENSE-2.0)
