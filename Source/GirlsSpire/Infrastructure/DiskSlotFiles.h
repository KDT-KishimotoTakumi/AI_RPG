#pragma once
#include "SaveSlot.h"
#include "SaveTransaction.h"
#include <filesystem>

namespace GirlsSpire {
class DiskSlotFiles : public SlotFiles,public JournalFiles {
public:
    explicit DiskSlotFiles(std::filesystem::path root) : root_(std::move(root)) {}
    SlotBytes Read(SaveKind kind,bool backup) override;
    bool Replace(SaveKind kind,bool backup,std::string_view bytes) override;
    SlotBytes ReadJournal() override;
    bool ReplaceJournal(std::string_view bytes) override;
    bool Preserve(SaveKind kind) override;
    bool PreserveJournal() override;
private:
    bool PreserveFiles(std::string_view prefix,std::initializer_list<std::filesystem::path> sources);
    std::optional<std::filesystem::path> Path(SaveKind kind,bool backup) const;
    SlotBytes ReadAt(const std::filesystem::path& path) const;
    bool ReplaceAt(const std::filesystem::path& path,std::string_view bytes) const;
    std::filesystem::path root_;
};
}
