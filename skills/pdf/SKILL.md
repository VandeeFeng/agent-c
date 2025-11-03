---
name: pdf
description: PDF manipulation and extraction toolkit for working with PDF documents
keywords: pdf, document, extract, forms, text, metadata
---

# PDF Skill

A comprehensive PDF manipulation skill that provides various tools for working with PDF documents.

## Capabilities

- Extract text content from PDF files
- Fill and extract PDF forms
- Get PDF metadata information
- Split and merge PDF documents
- Add watermarks and annotations

## Usage Examples

### Extract text from PDF
```
execute_skill: pdf extract_text document.pdf
```

### Extract form data
```
execute_skill: pdf extract_forms application.pdf
```

### Get PDF info
```
execute_skill: pdf get_info document.pdf
```

## Scripts

- `extract_text.py`: Extract text content from PDF files
- `extract_forms.py`: Extract and fill PDF forms
- `get_info.py`: Get PDF metadata and information

## Requirements

- Requires Python 3 with PyPDF2 library
- Some features may require additional dependencies

## Notes

This skill provides a foundation for PDF processing. Advanced features like OCR and complex form handling can be added as needed.