#if defined(__INTELLISENSE__) && !defined(MACHINE_IMAGE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IMAGE_SPLIT_AGGREGATOR)
#define MACHINE_IMAGE_SPLIT_INTELLISENSE_CONTEXT
#include "machine_image.c"
#else

int machine_image_report_refresh(MachineImageReport *report,
    MachineImageError *error) {
    MachineImageFile cloned_file;
    int ok;

    if (!report) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-148: invalid image report refresh contract");
        return 0;
    }
    machine_image_file_init(&cloned_file);
    ok = machine_image_clone_file(&report->file, &cloned_file, error) &&
        machine_image_build_report_from_file(&cloned_file, report, error);
    machine_image_file_free(&cloned_file);
    return ok;
}

int machine_image_verify_file(const MachineImageFile *image_file,
    MachineImageError *error) {
    size_t segment_index;
    size_t symbol_index;
    size_t relocation_index;

    if (!image_file) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-115: invalid image verify contract");
        return 0;
    }
    if (image_file->segment_count == 0u || !image_file->segments) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-116: image must contain at least one segment");
        return 0;
    }
    if (image_file->byte_count > 0u && !image_file->bytes) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-117: image byte payload is missing");
        return 0;
    }
    for (segment_index = 0u; segment_index < image_file->segment_count; ++segment_index) {
        const MachineImageSegment *segment = &image_file->segments[segment_index];

        if (!segment->name ||
            segment->image_offset + segment->byte_count > image_file->byte_count ||
            segment->align == 0u) {
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-118: invalid image segment");
            return 0;
        }
    }
    for (symbol_index = 0u; symbol_index < image_file->symbol_count; ++symbol_index) {
        const MachineImageSymbol *symbol = &image_file->symbols[symbol_index];

        if (!symbol->name) {
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-119: image symbol is unnamed");
            return 0;
        }
        if (symbol->is_defined) {
            if (symbol->segment_index >= image_file->segment_count) {
                machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-120: defined image symbol references invalid segment");
                return 0;
            }
            if (symbol->value_offset > image_file->segments[symbol->segment_index].byte_count ||
                symbol->virtual_address !=
                    image_file->segments[symbol->segment_index].virtual_address + symbol->value_offset) {
                machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-121: defined image symbol has invalid address");
                return 0;
            }
        } else if (symbol->segment_index != (size_t)-1 || symbol->virtual_address != 0u) {
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-122: undefined image symbol should not carry segment/address");
            return 0;
        }
    }
    if (image_file->has_entry) {
        const MachineImageSymbol *entry_symbol;

        if (image_file->entry_symbol_index >= image_file->symbol_count) {
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-123: image entry symbol index is out of range");
            return 0;
        }
        entry_symbol = &image_file->symbols[image_file->entry_symbol_index];
        if (!entry_symbol->is_defined ||
            entry_symbol->segment_index >= image_file->segment_count ||
            entry_symbol->virtual_address != image_file->entry_virtual_address) {
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-124: image entry symbol is invalid");
            return 0;
        }
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        const MachineImageRelocation *relocation = &image_file->relocations[relocation_index];

        if (!relocation->symbol_name ||
            relocation->segment_index >= image_file->segment_count ||
            relocation->segment_offset >= image_file->segments[relocation->segment_index].byte_count ||
            relocation->site_virtual_address !=
                image_file->segments[relocation->segment_index].virtual_address + (size_t)relocation->segment_offset ||
            relocation->symbol_index >= image_file->symbol_count) {
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-125: invalid image relocation");
            return 0;
        }
        if (relocation->is_resolved &&
            relocation->target_virtual_address == 0u) {
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-126: resolved image relocation is missing target address");
            return 0;
        }
        if (!relocation->is_resolved &&
            relocation->target_virtual_address != 0u) {
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-127: unresolved image relocation should not carry target address");
            return 0;
        }
    }
    return 1;
}

#endif
