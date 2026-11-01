const fs = require("node:fs");
const path = require("node:path");

const root = __dirname;
const modules = [
  "src/m3d_scene.cpp",
  "src/m3d_lighting.cpp",
  "src/m3d_intersection.cpp",
  "src/m3d_shading.cpp",
  "src/m3d_render.cpp",
  "src/m3d_post.cpp",
  "src/moteur3d_win32.cpp"
];

const sections = modules.map((file) => {
  const source = fs.readFileSync(path.join(root, file), "utf8");
  return `\n// ===== Reconstructed module: ${file} =====\n${source.trimEnd()}\n`;
});
const output = [
  "// Generated from src/*.cpp by assemble_native.js. Do not edit this generated file.",
  ...sections
].join("\n");

fs.writeFileSync(path.join(root, "moteur3d_v14_native.cpp"), output, "utf8");
console.log(`Assembled ${modules.length} modules into moteur3d_v14_native.cpp (${output.length} characters).`);
