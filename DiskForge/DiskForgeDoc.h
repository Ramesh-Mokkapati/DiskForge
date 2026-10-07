// DiskForgeDoc.h - Trivial SDI document (no file operations)
#pragma once

class CDiskForgeDoc : public CDocument
{
    DECLARE_DYNCREATE(CDiskForgeDoc)
public:
    CDiskForgeDoc() = default;
    virtual ~CDiskForgeDoc() = default;

protected:
    virtual BOOL OnNewDocument() { return CDocument::OnNewDocument(); }
    virtual void Serialize(CArchive&) {}

    DECLARE_MESSAGE_MAP()
};
