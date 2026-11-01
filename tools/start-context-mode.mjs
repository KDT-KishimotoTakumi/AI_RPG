import {readFileSync} from 'node:fs';
import {dirname,join} from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';

const root=dirname(dirname(fileURLToPath(import.meta.url)));
const packageRoot=join(root,'Saved','Tools','ContextMode','node_modules','context-mode');
const manifest=JSON.parse(readFileSync(join(packageRoot,'package.json'),'utf8'));
if(manifest.version!=='1.0.169')throw new Error('Context Modeの固定版が一致しません。');
// MCPの標準入出力を直接継承し、設定修復や自動更新を実行しない。
process.env.CONTEXT_MODE_PLATFORM='codex';
process.env.CONTEXT_MODE_PROJECT_DIR=root;
process.env.CONTEXT_MODE_DIR=join(root,'Saved','ContextMode');
process.chdir(root);
await import(pathToFileURL(join(packageRoot,'build','server.js')).href);
