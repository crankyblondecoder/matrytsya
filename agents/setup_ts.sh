#!/usr/bin/env bash
set -euo pipefail

# NOTE: Only use this for setting up a typescript env for LangGraph.

for cmd in node npm; do
  if ! command -v "$cmd" &>/dev/null; then
    echo "Error: '$cmd' is not installed or not in PATH." >&2
    exit 1
  fi
done

mkdir -p src

cat > package.json <<'EOF'
{
	"name": "matrytsya-build-agents",
	"version": "1.0.0",
	"type": "module",
	"scripts":
	{
		"proc_unit_tests": "tsx ./src/proc_unit_tests.ts"
	},
	"dependencies":
	{
		"@langchain/core": "^1.1.49",
		"@langchain/langgraph": "^1.4.2",
		"@langchain/ollama": "^1.2.7"
	},
	"devDependencies":
	{
		"@types/node": "^25.9.3",
	    "tsx": "^4.22.4",
		"typescript": "^6.0.3"
	},
	"allowScripts":
	{
		"esbuild": true
	}
}
EOF

cat > tsconfig.json <<'EOF'
{
  "compilerOptions":
  {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "outDir": "dist",
    "rootDir": "src",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "declaration": true,
    "sourceMap": true
  },
  "include": ["src/**/*"],
  "exclude": ["node_modules", "dist"]
}
EOF

cat > .gitignore <<'EOF'
node_modules/
dist/
EOF

rm -rf node_modules
npm install

echo "Done."
