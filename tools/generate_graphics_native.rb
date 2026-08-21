#!/usr/bin/env ruby
# frozen_string_literal: true

# Generates the target-selected graphics_native facade and the qualified
# aliases it imports from vulkan_native and metal_native. Keep this generator
# deliberately dependency-free so release archives can reproduce the facade.

ROOT = File.expand_path("..", __dir__)
START_MARKER = "// BEGIN GENERATED GRAPHICS NATIVE ALIASES"
END_MARKER = "// END GENERATED GRAPHICS NATIVE ALIASES"

Function = Struct.new(:name, :header, :arguments, keyword_init: true)

def split_parameters(value)
  parts = []
  current = +""
  depth = 0
  value.each_char do |character|
    if character == "," && depth.zero?
      parts << current.strip unless current.strip.empty?
      current.clear
      next
    end
    current << character
    depth += 1 if "([{".include?(character)
    depth -= 1 if ")]}".include?(character)
  end
  parts << current.strip unless current.strip.empty?
  parts
end

def native_functions(source)
  clean = source.split(START_MARKER, 2).first.rstrip
  functions = []
  offset = 0
  while (match = clean.match(/^pub fn (native_[a-z0-9_]+)\s*\(/, offset))
    open_brace = clean.index("{", match.begin(0))
    raise "missing body for #{match[1]}" unless open_brace

    header = clean[match.begin(0)...open_brace].strip
    signature = header.match(/\Apub fn (native_[a-z0-9_]+)\s*\((.*)\)\s*->\s*(.+)\z/m)
    raise "unsupported signature: #{header}" unless signature

    arguments = split_parameters(signature[2]).map do |parameter|
      parameter.split(":", 2).first.strip.sub(/\Amut\s+/, "")
    end
    functions << Function.new(name: signature[1], header: header, arguments: arguments)

    depth = 1
    index = open_brace + 1
    in_string = false
    escaped = false
    while index < clean.length && depth.positive?
      character = clean[index]
      if in_string
        if escaped
          escaped = false
        elsif character == "\\"
          escaped = true
        elsif character == '"'
          in_string = false
        end
      elsif character == '"'
        in_string = true
      elsif clean[index, 2] == "//"
        newline = clean.index("\n", index)
        index = newline || clean.length
        next
      elsif character == "{"
        depth += 1
      elsif character == "}"
        depth -= 1
      end
      index += 1
    end
    raise "unterminated body for #{match[1]}" unless depth.zero?
    offset = index
  end
  functions
end

def qualified_header(function, prefix)
  function.header.sub("pub fn #{function.name}", "pub fn #{prefix}#{function.name}")
end

def wrapper(function, exported_name, target_name)
  call = "#{target_name}(#{function.arguments.join(', ')})"
  "#{function.header.sub("pub fn #{function.name}", "pub fn #{exported_name}")} {\n    ret #{call}\n}"
end

def update_aliases(path, prefix, functions)
  source = File.read(path)
  clean = source.split(START_MARKER, 2).first.rstrip
  generated = functions.map do |function|
    wrapper(function, "#{prefix}#{function.name}", function.name)
  end.join("\n")
  File.write(path, "#{clean}\n\n#{START_MARKER}\n#{generated}\n#{END_MARKER}\n")
end

metal_path = File.join(ROOT, "workspace/metal_native/src/lib.enq")
vulkan_path = File.join(ROOT, "workspace/vulkan_native/src/lib.enq")
functions = native_functions(File.read(metal_path))
vulkan_functions = native_functions(File.read(vulkan_path))
unless functions.map(&:name) == vulkan_functions.map(&:name)
  abort "metal_native and vulkan_native public native APIs differ"
end

update_aliases(metal_path, "metal_", functions)
update_aliases(vulkan_path, "vulkan_", vulkan_functions)

metal_names = functions.map { |function| "metal_#{function.name}" }.join(", ")
vulkan_names = functions.map { |function| "vulkan_#{function.name}" }.join(", ")
facade = +<<~ENCORE
  //! Compile-time selected low-level graphics API.
  //!
  //! Vulkan remains the implementation on non-macOS targets. macOS resolves
  //! the same resource and frame contract to the native Metal package.

  import metal_native::{#{metal_names}}
  import vulkan_native::{#{vulkan_names}}

ENCORE

functions.each do |function|
  facade << "#cfg(target_os = \"macos\")\n"
  facade << wrapper(function, function.name, "metal_#{function.name}") << "\n"
  facade << "#cfg(not(target_os = \"macos\"))\n"
  facade << wrapper(function, function.name, "vulkan_#{function.name}") << "\n"
end

facade_root = File.join(ROOT, "workspace/graphics_native")
Dir.mkdir(facade_root) unless Dir.exist?(facade_root)
source_root = File.join(facade_root, "src")
Dir.mkdir(source_root) unless Dir.exist?(source_root)
File.write(File.join(source_root, "lib.enq"), facade)

puts "Generated #{functions.length} graphics-native facade functions"
