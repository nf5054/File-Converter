"""
OmniConvert Document Conversion & Compression Engine
Outputs real-time PROGRESS:<percent>:<detail> messages to stdout for live UI updates.
"""

import sys
import os
import argparse
import io

# Ensure UTF-8 output encoding for all stdout and stderr streams
try:
    if sys.stdout:
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    if sys.stderr:
        sys.stderr.reconfigure(encoding='utf-8', errors='replace')
except Exception:
    pass

def log_progress(pct, msg):
    print(f"PROGRESS:{pct}:{msg}", flush=True)

def convert_word_to_pdf(input_path, output_path):
    log_progress(10, "Initializing Word conversion engine...")
    try:
        import win32com.client
        log_progress(25, "Opening Word document...")
        word = win32com.client.DispatchEx("Word.Application")
        word.Visible = False
        word.DisplayAlerts = 0
        doc = word.Documents.Open(os.path.abspath(input_path), ReadOnly=True)
        log_progress(60, "Exporting to PDF format with fixed layout...")
        doc.ExportAsFixedFormat(os.path.abspath(output_path), 17)
        doc.Close(False)
        word.Quit()
        log_progress(100, "Word to PDF conversion complete!")
        return True
    except Exception as e:
        print(f"Word COM notice: {e}", file=sys.stderr)

    try:
        log_progress(30, "Using fallback document parser...")
        import docx
        from reportlab.lib.pagesizes import letter
        from reportlab.pdfgen import canvas
        
        doc = docx.Document(input_path)
        c = canvas.Canvas(output_path, pagesize=letter)
        width, height = letter
        y = height - 50
        
        total_p = len(doc.paragraphs)
        for i, p in enumerate(doc.paragraphs):
            text = p.text.strip()
            if not text:
                y -= 14
                continue
            if y < 50:
                c.showPage()
                y = height - 50
            c.setFont("Helvetica", 11)
            c.drawString(50, y, text[:90])
            y -= 18
            if total_p > 0 and i % 5 == 0:
                pct = 30 + int((i / total_p) * 60)
                log_progress(pct, f"Rendering paragraph {i+1} of {total_p}...")
            
        c.save()
        log_progress(100, "Document converted to PDF successfully!")
        return True
    except Exception as e:
        print(f"Word fallback failed: {e}", file=sys.stderr)
        return False

def convert_pdf_to_word(input_path, output_path):
    log_progress(10, "Loading PDF structure and layout...")
    try:
        from pdf2docx import Converter
        cv = Converter(input_path)
        log_progress(35, "Parsing pages and extracting text, tables, and images...")
        cv.convert(output_path, start=0, end=None)
        cv.close()
        log_progress(100, "PDF to Word DOCX conversion complete!")
        return True
    except Exception as e:
        print(f"PDF to Word error: {e}", file=sys.stderr)
        return False

def convert_pptx_to_pdf(input_path, output_path):
    log_progress(15, "Opening PowerPoint presentation...")
    try:
        import win32com.client
        powerpoint = win32com.client.DispatchEx("PowerPoint.Application")
        powerpoint.DisplayAlerts = 0
        deck = powerpoint.Presentations.Open(os.path.abspath(input_path), WithWindow=False)
        log_progress(60, "Exporting presentation slides to PDF...")
        deck.SaveAs(os.path.abspath(output_path), 32)
        deck.Close()
        powerpoint.Quit()
        log_progress(100, "PowerPoint to PDF conversion complete!")
        return True
    except Exception as e:
        print(f"PowerPoint COM error: {e}", file=sys.stderr)
        return False

def convert_txt_to_pdf(input_path, output_path):
    log_progress(10, "Reading text file...")
    try:
        from reportlab.lib.pagesizes import letter
        from reportlab.pdfgen import canvas
        
        c = canvas.Canvas(output_path, pagesize=letter)
        width, height = letter
        y = height - 50
        
        with open(input_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        total = len(lines)
        for i, line in enumerate(lines):
            if y < 50:
                c.showPage()
                y = height - 50
            line = line.rstrip('\r\n')
            c.setFont("Helvetica", 10)
            c.drawString(50, y, line[:100])
            y -= 15
            if total > 0 and i % 50 == 0:
                pct = 10 + int((i / total) * 85)
                log_progress(pct, f"Rendering line {i+1} of {total}...")
                
        c.save()
        log_progress(100, "Text to PDF complete!")
        return True
    except Exception as e:
        print(f"TXT to PDF error: {e}", file=sys.stderr)
        return False

def convert_pdf_to_txt(input_path, output_path):
    log_progress(10, "Opening PDF for text extraction...")
    try:
        import pymupdf
        doc = pymupdf.open(input_path)
        total = len(doc)
        text = ""
        for i, page in enumerate(doc):
            text += page.get_text() + "\n"
            pct = 10 + int(((i + 1) / total) * 80)
            log_progress(pct, f"Extracting text from page {i+1} of {total}...")
        doc.close()
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(text)
        log_progress(100, "Text extraction complete!")
        return True
    except Exception as e:
        print(f"PDF to TXT error: {e}", file=sys.stderr)
        return False

def compress_pdf_file(input_path, output_path, quality_pct=75, target_ppi=150):
    log_progress(5, "Analyzing PDF structure and raster images...")
    try:
        import pymupdf
        import PIL.Image

        doc = pymupdf.open(input_path)
        total_pages = len(doc)
        if total_pages == 0:
            return False

        processed_xrefs = set()
        
        # Count total images to process
        all_img_xrefs = []
        for pno in range(total_pages):
            page = doc[pno]
            for img_info in page.get_images():
                xref = img_info[0]
                if xref not in processed_xrefs:
                    processed_xrefs.add(xref)
                    all_img_xrefs.append((pno, xref))

        total_imgs = len(all_img_xrefs)
        log_progress(12, f"Found {total_pages} page(s) and {total_imgs} embedded image(s)...")

        # Downsample and recompress images
        img_counter = 0
        for pno, xref in all_img_xrefs:
            img_counter += 1
            pct = 12 + int((img_counter / max(1, total_imgs)) * 70)
            log_progress(pct, f"Compressing image {img_counter} of {total_imgs} (target {target_ppi} PPI, {quality_pct}% quality)...")

            try:
                base_img = doc.extract_image(xref)
                if not base_img or not base_img.get("image"):
                    continue

                raw_bytes = base_img["image"]
                pil_img = PIL.Image.open(io.BytesIO(raw_bytes))
                orig_w, orig_h = pil_img.size

                scale = min(1.0, float(target_ppi) / 150.0)
                new_w = max(16, int(orig_w * scale))
                new_h = max(16, int(orig_h * scale))

                if new_w < orig_w and new_h < orig_h:
                    pil_img = pil_img.resize((new_w, new_h), PIL.Image.Resampling.LANCZOS)

                if pil_img.mode in ("RGBA", "P"):
                    pil_img = pil_img.convert("RGB")

                buf = io.BytesIO()
                pil_img.save(buf, format="JPEG", quality=int(quality_pct), optimize=True)
                new_bytes = buf.getvalue()

                if len(new_bytes) < len(raw_bytes):
                    page = doc[pno]
                    page.replace_image(xref, stream=new_bytes)
            except Exception:
                continue

        log_progress(85, "Optimizing and deflating document streams...")
        doc.save(output_path, garbage=4, deflate=True, clean=True)
        doc.close()
        log_progress(100, "PDF compression finished successfully!")
        return True
    except Exception as e:
        print(f"PDF compression exception: {e}", file=sys.stderr)
        return False

def main():
    parser = argparse.ArgumentParser(description="OmniConvert Document Engine")
    parser.add_argument("--input", required=True, help="Input file path")
    parser.add_argument("--output", required=True, help="Output file path")
    parser.add_argument("--compress", action="store_true", help="Perform PDF compression")
    parser.add_argument("--quality", type=int, default=75, help="Image quality percent (1-100)")
    parser.add_argument("--ppi", type=int, default=150, help="Target PPI resolution (72, 144, 150, 300)")
    args = parser.parse_args()

    success = False

    if args.compress:
        success = compress_pdf_file(args.input, args.output, args.quality, args.ppi)
    else:
        in_ext = os.path.splitext(args.input)[1].lower().replace('.', '')
        out_ext = os.path.splitext(args.output)[1].lower().replace('.', '')

        if in_ext in ['docx', 'doc'] and out_ext == 'pdf':
            success = convert_word_to_pdf(args.input, args.output)
        elif in_ext == 'pdf' and out_ext in ['docx', 'doc']:
            success = convert_pdf_to_word(args.input, args.output)
        elif in_ext in ['pptx', 'ppt'] and out_ext == 'pdf':
            success = convert_pptx_to_pdf(args.input, args.output)
        elif in_ext in ['txt', 'md'] and out_ext == 'pdf':
            success = convert_txt_to_pdf(args.input, args.output)
        elif in_ext == 'pdf' and out_ext in ['txt', 'md']:
            success = convert_pdf_to_txt(args.input, args.output)
        else:
            print(f"Unsupported document transformation: {in_ext} -> {out_ext}", file=sys.stderr)
            sys.exit(1)

    if success and os.path.exists(args.output):
        orig_sz = os.path.getsize(args.input)
        new_sz = os.path.getsize(args.output)
        print(f"SUCCESS:{args.output}:{orig_sz}:{new_sz}", flush=True)
        sys.exit(0)
    else:
        print("OPERATION_FAILED", file=sys.stderr, flush=True)
        sys.exit(1)

if __name__ == "__main__":
    main()
