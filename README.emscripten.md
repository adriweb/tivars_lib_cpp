### Template for the generated .js (wasm loader):

```js
const lib = function() {
  let _scriptDir = typeof document !== 'undefined' && document.currentScript ? document.currentScript.src : undefined;
  if (typeof __filename !== 'undefined') _scriptDir = _scriptDir || __filename;
  let Module = {};
  
  var readyPromiseResolve,readyPromiseReject;Module["ready"] = ... run();

  Module["FS"] = FS;
  return Module.ready;
};

export { lib };
```

### Using it in a page:

```html
<script type="module">
    import { lib } from './tivars_lib.js';
    window.TIVarsLib = await lib();
</script>
```
