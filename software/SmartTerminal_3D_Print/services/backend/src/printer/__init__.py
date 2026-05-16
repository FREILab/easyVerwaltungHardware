from fastapi import APIRouter

router = APIRouter(tags=["printer-api"])


@router.get("/health")
def health() -> dict:
	return {"status": "ok", "service": "printer-emulation", "mode": "scaffold"}


@router.get("/printers")
def list_printers() -> dict:
	return {
		"items": [],
		"note": "PrusaLink-Emulation/Forwarding ist im Geruest vorbereitet.",
	}

