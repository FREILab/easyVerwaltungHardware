from fastapi import FastAPI
from api import router as api_router
from printer import router as printer_router

app = FastAPI(title="Smart Terminal 3D-Print Backend")

# Interne API für die UI (Session, Jobs, Payment)
app.include_router(api_router, prefix="/api")

# PrusaLink-Emulation: Slicer sendet hierhin (Port 8080 → 8000 intern)
app.include_router(printer_router, prefix="/api/v1")
