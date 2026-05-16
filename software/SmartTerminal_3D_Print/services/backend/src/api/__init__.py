from fastapi import APIRouter

router = APIRouter(tags=["ui-api"])


@router.get("/health")
def health() -> dict:
	return {"status": "ok", "service": "backend", "mode": "scaffold"}


@router.get("/jobs")
def list_jobs() -> dict:
	# Scaffold response that matches planned UI contract from the wiki.
	return {
		"items": [],
		"note": "Job-Queue ist im Geruest noch nicht implementiert.",
	}


@router.get("/session")
def session_state() -> dict:
	return {
		"active": False,
		"note": "RFID-Session-Management folgt in einem spaeteren Schritt.",
	}


@router.post("/jobs/{job_id}/start")
def start_job(job_id: str) -> dict:
	return {
		"accepted": False,
		"jobId": job_id,
		"note": "Druckstart/Abrechnung ist als naechstes Feature vorgesehen.",
	}
