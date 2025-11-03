#!/usr/bin/env python3
"""
PDF Form Extraction Script
Extracts form data from PDF documents
"""

import sys
import os
try:
    import PyPDF2
except ImportError:
    print("Error: PyPDF2 is required. Install with: pip install PyPDF2")
    sys.exit(1)

def extract_form_fields(pdf_path):
    """Extract form field information from PDF"""
    try:
        with open(pdf_path, 'rb') as file:
            pdf_reader = PyPDF2.PdfReader(file)

            if pdf_reader.get_fields():
                print(f"Form fields found in {pdf_path}:")
                for field_name, field in pdf_reader.get_fields().items():
                    field_type = getattr(field, '/FT', 'Unknown')
                    field_value = getattr(field, '/V', '')
                    print(f"  - {field_name} ({field_type}): {field_value}")
            else:
                print(f"No form fields found in {pdf_path}")

            # Also print basic PDF info
            print(f"\nPDF Info:")
            print(f"  Pages: {len(pdf_reader.pages)}")
            print(f"  Title: {pdf_reader.metadata.get('/Title', 'Unknown') if pdf_reader.metadata else 'No metadata'}")

    except Exception as e:
        print(f"Error processing PDF: {e}")
        return False

    return True

def main():
    if len(sys.argv) != 2:
        print("Usage: extract_forms.py <pdf_file>")
        sys.exit(1)

    pdf_file = sys.argv[1]

    if not os.path.exists(pdf_file):
        print(f"Error: File '{pdf_file}' not found")
        sys.exit(1)

    if not pdf_file.lower().endswith('.pdf'):
        print("Error: Please provide a PDF file")
        sys.exit(1)

    print(f"Extracting form data from: {pdf_file}")
    extract_form_fields(pdf_file)

if __name__ == "__main__":
    main()