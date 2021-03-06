#include <string.h>
#include <stdio.h>
#include "kip.h"
#include "npdm.h"

void ini1_process(ini1_ctx_t *ctx) {
    /* Read *just* safe amount. */
    ini1_header_t raw_header; 
    fseeko64(ctx->file, 0, SEEK_SET);
    if (fread(&raw_header, 1, sizeof(raw_header), ctx->file) != sizeof(raw_header)) {
        fprintf(stderr, "Failed to read INI1 header!\n");
        exit(EXIT_FAILURE);
    }
    
    if (raw_header.magic != MAGIC_INI1 || raw_header.num_processes > INI1_MAX_KIPS) {
        printf("Error: INI1 is corrupt!\n");
        exit(EXIT_FAILURE);
    }

    ctx->header = malloc(raw_header.size);
    if (ctx->header == NULL) {
        fprintf(stderr, "Failed to allocate INI1 header!\n");
        exit(EXIT_FAILURE);
    }
    
    fseeko64(ctx->file, 0, SEEK_SET);
    if (fread(ctx->header, 1, raw_header.size, ctx->file) != raw_header.size) {
        fprintf(stderr, "Failed to read INI1!\n");
        exit(EXIT_FAILURE);
    }
    
    uint64_t offset = 0;
    for (unsigned int i = 0; i < ctx->header->num_processes; i++) {
        ctx->kips[i].tool_ctx = ctx->tool_ctx;
        ctx->kips[i].header = (kip1_header_t *)&ctx->header->kip_data[offset];
        if (ctx->kips[i].header->magic != MAGIC_KIP1) {
            fprintf(stderr, "INI1 is corrupted!\n");
            exit(EXIT_FAILURE);
        }
        offset += kip1_get_size(&ctx->kips[i]);
    }
    
    if (ctx->tool_ctx->action & ACTION_INFO) {
        ini1_print(ctx);
    }
    
    if (ctx->tool_ctx->action & ACTION_EXTRACT) {
        ini1_save(ctx);
    }
}

void ini1_print(ini1_ctx_t *ctx) {
    printf("INI1:\n");
    printf("    Number of Processes:            %02"PRIx32"\n", ctx->header->num_processes);
    printf("    Size:                           %08"PRIx32"\n", ctx->header->size);
    printf("\n");
    for (unsigned int i = 0; i < ctx->header->num_processes; i++) {
        printf("Process %02"PRIx32":\n", i);
        kip1_print(&ctx->kips[i], 1);
        printf("\n");
    }
    printf("\n");
}

void ini1_save(ini1_ctx_t *ctx) {
    filepath_t *dirpath = NULL;
    if (ctx->tool_ctx->file_type == FILETYPE_INI1 && ctx->tool_ctx->settings.out_dir_path.enabled) {
        dirpath = &ctx->tool_ctx->settings.out_dir_path.path;
    }
    if (dirpath == NULL || dirpath->valid != VALIDITY_VALID) {
        dirpath = &ctx->tool_ctx->settings.ini1_dir_path;
    }
    if (dirpath != NULL && dirpath->valid == VALIDITY_VALID) {
        os_makedir(dirpath->os_path);
        for (unsigned int i = 0; i < ctx->header->num_processes; i++) {
            char padded_name[0x20];
            memset(&padded_name, 0, sizeof(padded_name));
            memcpy(&padded_name, ctx->kips[i].header->name, sizeof(ctx->kips[i].header->name));
            strcat(padded_name, ".kip1");
            printf("Saving %s to %s/%s...\n", padded_name, dirpath->char_path, padded_name);
            save_buffer_to_directory_file(ctx->kips[i].header, kip1_get_size(&ctx->kips[i]), dirpath, padded_name);
        }
    }
}

void kip1_process(kip1_ctx_t *ctx) {
    /* Read *just* safe amount. */
    kip1_header_t raw_header; 
    fseeko64(ctx->file, 0, SEEK_SET);
    if (fread(&raw_header, 1, sizeof(raw_header), ctx->file) != sizeof(raw_header)) {
        fprintf(stderr, "Failed to read KIP1 header!\n");
        exit(EXIT_FAILURE);
    }
    
    if (raw_header.magic != MAGIC_KIP1) {
        printf("Error: KIP1 is corrupt!\n");
        exit(EXIT_FAILURE);
    }

    uint64_t size = kip1_get_size_from_header(&raw_header);
    ctx->header = malloc(size);
    if (ctx->header == NULL) {
        fprintf(stderr, "Failed to allocate KIP1 header!\n");
        exit(EXIT_FAILURE);
    }
    
    fseeko64(ctx->file, 0, SEEK_SET);
    if (fread(ctx->header, 1, size, ctx->file) != size) {
        fprintf(stderr, "Failed to read KIP1!\n");
        exit(EXIT_FAILURE);
    }
    
    if (ctx->tool_ctx->action & ACTION_INFO) {
        kip1_print(ctx, 0);
    }
    
    if (ctx->tool_ctx->action & ACTION_EXTRACT) {
        kip1_save(ctx);
    }
}

void kip1_print(kip1_ctx_t *ctx, int suppress) {
    if (!suppress) printf("KIP1:\n");
    printf("    Title ID:                       %016"PRIx64"\n", ctx->header->title_id);
    char padded_name[13];
    memset(&padded_name, 0, sizeof(padded_name));
    memcpy(&padded_name, ctx->header->name, sizeof(ctx->header->name));
    printf("    Name:                           %s\n", padded_name);
    printf("    Process Category:               %s\n", npdm_get_proc_category(ctx->header->process_category));
    printf("    Main Thread Priority:           %"PRId8"\n", ctx->header->main_thread_priority);
    printf("    Default CPU Core:               %"PRId8"\n", ctx->header->default_core);
    printf("    Is 64 Bit:                      %s\n", (ctx->header->flags & (1 << 3)) ? "True" : "False");
    printf("    Is Address Space 64 Bit:        %s\n", (ctx->header->flags & (1 << 4)) ? "True" : "False");
    printf("    Sections:\n");
    printf("        .text:                      %08"PRIx32"-%08"PRIx32"\n", ctx->header->section_headers[0].out_offset, ctx->header->section_headers[0].out_offset + align(ctx->header->section_headers[0].out_size, 0x1000));
    printf("        .rodata:                    %08"PRIx32"-%08"PRIx32"\n", ctx->header->section_headers[1].out_offset, ctx->header->section_headers[1].out_offset + align(ctx->header->section_headers[1].out_size, 0x1000));
    printf("        .rwdata:                    %08"PRIx32"-%08"PRIx32"\n", ctx->header->section_headers[2].out_offset, ctx->header->section_headers[2].out_offset + align(ctx->header->section_headers[2].out_size, 0x1000));
    printf("        .bss:                       %08"PRIx32"-%08"PRIx32"\n", ctx->header->section_headers[3].out_offset, ctx->header->section_headers[3].out_offset + align(ctx->header->section_headers[3].out_size, 0x1000));
    printf("    Kernel Access Control:\n");
    kac_print(ctx->header->capabilities, 0x20);
    printf("\n");
}

void kip1_save(kip1_ctx_t *ctx) {
    /* Do nothing. */
}