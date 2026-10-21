// A bundled module requires the bundle's own entry and a module that exports
// only types: esbuild reads them as `(init_index(), __toCommonJS(index_exports))`
// and `(init_manifest(), __toCommonJS(manifest_exports))`, the second an
// empty namespace whose names all read undefined.
import { KIND, late } from "gtemptyns";

console.log(KIND, late());
