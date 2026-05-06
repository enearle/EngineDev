#define _CRT_SECURE_NO_WARNINGS
#include <../spirv_cross_c.h>
#include "spirv-tools/optimizer.hpp"
#include "spirv-tools/libspirv.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------

static void check(spvc_result result, spvc_context ctx, const char *msg) {
    if (result != SPVC_SUCCESS) {
        fprintf(stderr, "[spvc] %s: %s\n", msg, spvc_context_get_last_error_string(ctx));
        spvc_context_destroy(ctx);
        exit(1);
    }
}

static void spirv_cross_error_cb(void *userdata, const char *error) {
    (void)userdata;
    fprintf(stderr, "[spvc error] %s\n", error);
}

static std::vector<uint32_t> read_spirv(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open: %s\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long byte_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (byte_size % 4 != 0) {
        fprintf(stderr, "File size not aligned to 4 bytes — not valid SPIR-V: %s\n", path);
        fclose(f);
        exit(1);
    }

    std::vector<uint32_t> words(byte_size / 4);
    fread(words.data(), 4, words.size(), f);
    fclose(f);
    return words;
}

// ------------------------------------------------------------
// Step 1: Optimize SPIR-V to fix unnamed composites (<value>.xxx)
// ------------------------------------------------------------

static std::vector<uint32_t> optimize_spirv(const std::vector<uint32_t> &spirv) {
    spvtools::Optimizer opt(SPV_ENV_VULKAN_1_1);

    // Suppress noisy optimizer output unless you want it
    opt.SetMessageConsumer([](spv_message_level_t level,
                               const char * /*source*/,
                               const spv_position_t & /*position*/,
                               const char *message) {
        if (level <= SPV_MSG_WARNING) {
            fprintf(stderr, "[spirv-opt] %s\n", message);
        }
    });

    // RegisterPerformancePasses inlines composites and eliminates
    // the unnamed SSA temporaries that cause <value>.xxx in HLSL output
    opt.RegisterPass(spvtools::Optimizer::PassToken(spvtools::CreateInlineExhaustivePass()));
    opt.RegisterPass(spvtools::Optimizer::PassToken(spvtools::CreateLocalAccessChainConvertPass()));
    opt.RegisterPass(spvtools::Optimizer::PassToken(spvtools::CreateLocalSingleBlockLoadStoreElimPass()));
    opt.RegisterPass(spvtools::Optimizer::PassToken(spvtools::CreateLocalSingleStoreElimPass()));
    opt.RegisterPass(spvtools::Optimizer::PassToken(spvtools::CreateAggressiveDCEPass()));
    opt.RegisterPass(spvtools::Optimizer::PassToken(spvtools::CreateCompactIdsPass()));

    std::vector<uint32_t> optimized;
    if (!opt.Run(spirv.data(), spirv.size(), &optimized)) {
        fprintf(stderr, "[spirv-opt] Optimization failed, falling back to raw SPIR-V\n");
        return spirv;
    }

    return optimized;
}

// ------------------------------------------------------------
// Step 2: Compile SPIR-V -> HLSL using SPIRV-Cross C API
// ------------------------------------------------------------

static const char *compile_to_hlsl(spvc_context ctx,
                                    const std::vector<uint32_t> &spirv,
                                    unsigned shader_model,
                                    uint32_t register_binding,
                                    uint32_t register_space) {
    spvc_result result;

    // Parse SPIR-V words into an IR representation
    spvc_parsed_ir ir = nullptr;
    result = spvc_context_parse_spirv(ctx, spirv.data(), spirv.size(), &ir);
    check(result, ctx, "spvc_context_parse_spirv");

    // Create the HLSL backend compiler.
    // SPVC_CAPTURE_MODE_TAKE_OWNERSHIP: the compiler owns the IR,
    // freeing it when the context is destroyed.
    spvc_compiler compiler = nullptr;
    result = spvc_context_create_compiler(
        ctx,
        SPVC_BACKEND_HLSL,
        ir,
        SPVC_CAPTURE_MODE_TAKE_OWNERSHIP,
        &compiler
    );
    check(result, ctx, "spvc_context_create_compiler");

    // Build options object
    spvc_compiler_options opts = nullptr;
    result = spvc_compiler_create_compiler_options(compiler, &opts);
    check(result, ctx, "spvc_compiler_create_compiler_options");

    // Shader model: 50 = SM5.0 (FXC), 60 = SM6.0 (DXC)
    spvc_compiler_options_set_uint(opts, SPVC_COMPILER_OPTION_HLSL_SHADER_MODEL, shader_model);

    // Helps if you're mixing with point primitives
    spvc_compiler_options_set_bool(opts, SPVC_COMPILER_OPTION_HLSL_POINT_SIZE_COMPAT,  SPVC_TRUE);
    spvc_compiler_options_set_bool(opts, SPVC_COMPILER_OPTION_HLSL_POINT_COORD_COMPAT, SPVC_TRUE);

    result = spvc_compiler_install_compiler_options(compiler, opts);
    check(result, ctx, "spvc_compiler_install_compiler_options");

    spvc_hlsl_resource_binding binding;
    spvc_hlsl_resource_binding_init(&binding);
    binding.stage = SpvExecutionModelVertex;
    binding.desc_set = SPVC_HLSL_PUSH_CONSTANT_DESC_SET;
    binding.binding = SPVC_HLSL_PUSH_CONSTANT_BINDING;
    binding.cbv.register_space = register_space;
    binding.cbv.register_binding = register_binding;
    spvc_compiler_hlsl_add_resource_binding(compiler, &binding);

    binding.stage = SpvExecutionModelFragment;
    spvc_compiler_hlsl_add_resource_binding(compiler, &binding);

    // Vertex attribute remapping (add after installing options)
    spvc_hlsl_vertex_attribute_remap remaps[7] = {
        { .location = 0, .semantic = "POSITION" },
        { .location = 1, .semantic = "NORMAL" },
        { .location = 2, .semantic = "TANGENT" },
        { .location = 3, .semantic = "BINORMAL" },
        { .location = 4, .semantic = "TEXCOORD" },
        { .location = 5, .semantic = "BLENDWEIGHT" },
        { .location = 6, .semantic = "BLENDINDICES" }
    };

    for (int i = 0; i < 7; i++) {
        spvc_compiler_hlsl_add_vertex_attribute_remap(compiler, &remaps[i], 1);
    }
    // NOTE: gl_Position -> SV_Position remapping is automatic when the
    // SPIR-V has OpDecorate %gl_Position BuiltIn Position.
    // If it's still wrong after this, run: spirv-dis your.spv | grep BuiltIn
    // to verify the decoration exists in the binary.

    // Compile to HLSL source. The string lifetime is tied to the context.
    const char *hlsl_source = nullptr;
    result = spvc_compiler_compile(compiler, &hlsl_source);
    check(result, ctx, "spvc_compiler_compile");

    return hlsl_source;
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <input.spv> <output.hlsl> <register_binding> <register_space> [shader_model=61]\n", argv[0]);
        return 1;
    }

    const char *input_path  = argv[1];
    const char *output_path = argv[2];
    uint32_t register_binding = atoi(argv[3]);
    uint32_t register_space = atoi(argv[4]);
    unsigned shader_model = (argc >= 6) ? (unsigned)atoi(argv[5]) : 61;

    // Read raw SPIR-V
    std::vector<uint32_t> spirv = read_spirv(input_path);
    fprintf(stdout, "Read %zu words from %s\n", spirv.size(), input_path);

    // Optimize: fixes <value>.xxx composite/swizzle issue
    std::vector<uint32_t> optimized = optimize_spirv(spirv);
    fprintf(stdout, "Optimized: %zu -> %zu words\n", spirv.size(), optimized.size());

    // Set up SPIRV-Cross context
    spvc_context ctx = nullptr;
    if (spvc_context_create(&ctx) != SPVC_SUCCESS) {
        fprintf(stderr, "Failed to create spvc context\n");
        return 1;
    }
    spvc_context_set_error_callback(ctx, spirv_cross_error_cb, nullptr);

    // Compile to HLSL
    const char *hlsl = compile_to_hlsl(ctx, optimized, shader_model, register_binding, register_space);

    // Write output
    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "Cannot write to: %s\n", output_path);
        spvc_context_destroy(ctx);
        return 1;
    }
    fputs(hlsl, out);
    fclose(out);

    fprintf(stdout, "HLSL (SM%u) written to %s\n", shader_model, output_path);

    spvc_context_destroy(ctx);
    return 0;
}
