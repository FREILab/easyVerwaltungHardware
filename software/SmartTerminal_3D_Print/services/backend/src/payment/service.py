from __future__ import annotations

import time
import uuid
from dataclasses import dataclass, field
from typing import Any


@dataclass
class TerminalSession:
    session_id: str
    payment_authorization_id: str
    rfid_uid: str  # Kept in RAM only, never persisted or logged
    user: dict[str, Any]
    balance_cents: int
    currency: str
    approved_for_3d_print: bool
    is_instructor: bool
    expires_at: float
    transactions: list[dict[str, Any]] = field(default_factory=list)

    @property
    def is_expired(self) -> bool:
        return time.time() >= self.expires_at

    def public_dict(self) -> dict[str, Any]:
        return {
            "sessionId": self.session_id,
            "paymentAuthorizationId": self.payment_authorization_id,
            "user": self.user,
            "balanceCents": self.balance_cents,
            "currency": self.currency,
            "approved": self.approved_for_3d_print,
            "isInstructor": self.is_instructor,
            "expiresInSeconds": max(0, int(round(self.expires_at - time.time()))),
            "transactions": self.transactions[:10],
        }


class PaymentProvider:
    """Contract matching easyVerwaltung payment-service.md."""

    async def health(self) -> dict[str, Any]:
        raise NotImplementedError

    async def get_metadata(self) -> dict[str, Any]:
        raise NotImplementedError

    async def authorize(self, *, mode: str, rfid: str, pin: str | None = None) -> dict[str, Any]:
        raise NotImplementedError

    async def charge(
        self,
        *,
        payment_authorization_id: str,
        amount_cents: int,
        items: list[dict[str, Any]],
        idempotency_key: str,
        correlation_id: str,
        allow_overdraft: bool = False,
        max_overdraft_cents: int = 0,
    ) -> dict[str, Any]:
        raise NotImplementedError

    async def cancel_authorization(self, *, payment_authorization_id: str, reason: str) -> dict[str, Any]:
        raise NotImplementedError

    async def topup_mock(self, *, rfid: str, amount_cents: int) -> dict[str, Any]:
        raise NotImplementedError

    async def approve_user_mock(self, *, target_rfid: str, instructor_rfid: str) -> dict[str, Any]:
        raise NotImplementedError


FREILAB_SECTIONS: list[dict[str, Any]] = [
    {"sectionId": "sec_3d", "name": "3D-Druck", "description": "3D-Druck", "pictogram": "printer", "status": "active"},
    {"sectionId": "sec_elec", "name": "Elektronik", "description": "Elektronik", "pictogram": "cpu", "status": "active"},
    {"sectionId": "sec_wood", "name": "Holzwerkstatt", "description": "Holzwerkstatt", "pictogram": "wood", "status": "active"},
    {"sectionId": "sec_metal", "name": "Metall", "description": "Metall", "pictogram": "wrench", "status": "active"},
    {"sectionId": "sec_bike", "name": "Fahrrad Bereich", "description": "Fahrrad Bereich", "pictogram": "bike", "status": "active"},
    {"sectionId": "sec_laser", "name": "Lasercutter", "description": "Lasercutter", "pictogram": "laser", "status": "active"},
    {"sectionId": "sec_cnc", "name": "CNC", "description": "CNC", "pictogram": "cnc", "status": "active"},
    {"sectionId": "sec_weld", "name": "Schweißen", "description": "Schweißen", "pictogram": "flame", "status": "active"},
    {"sectionId": "sec_textile", "name": "Textil", "description": "Textil", "pictogram": "scissors", "status": "active"},
]


class MockPaymentProvider(PaymentProvider):
    """Realistic in-memory mirror of easyVerwaltung Payment API for local & Pi testing."""

    def __init__(self) -> None:
        self._users: dict[str, dict[str, Any]] = {
            "04AABBCCDD01": {
                "member_id": "mem_001",
                "user_id": "usr_marius",
                "display_name": "Marius Test",
                "first_name": "Marius",
                "last_name": "Test",
                "pin": "1234",
                "balance_cents": 2500,
                "approved": True,
                "is_instructor": False,
                "transactions": [
                    {"ts": time.time() - 86400, "title": "Aufladung Guthaben", "amountCents": 2500},
                ],
            },
            "04AABBCCDD02": {
                "member_id": "mem_002",
                "user_id": "usr_erika",
                "display_name": "Erika Wenig",
                "first_name": "Erika",
                "last_name": "Wenig",
                "pin": "1234",
                "balance_cents": 15,
                "approved": True,
                "is_instructor": False,
                "transactions": [
                    {"ts": time.time() - 7200, "title": "3D-Druck Benchy", "amountCents": -185},
                ],
            },
            "04AABBCCDD03": {
                "member_id": "mem_003",
                "user_id": "usr_neuling",
                "display_name": "Alex Neuling",
                "first_name": "Alex",
                "last_name": "Neuling",
                "pin": "1234",
                "balance_cents": 1000,
                "approved": False,
                "is_instructor": False,
                "transactions": [],
            },
            "04AABBCCDD99": {
                "member_id": "mem_099",
                "user_id": "usr_bv",
                "display_name": "Sam Einweiser (BV)",
                "first_name": "Sam",
                "last_name": "Einweiser",
                "pin": "9999",
                "balance_cents": 5000,
                "approved": True,
                "is_instructor": True,
                "transactions": [],
            },
        }
        self._authorizations: dict[str, dict[str, Any]] = {}
        self._idempotency: dict[str, dict[str, Any]] = {}
        self._sections = list(FREILAB_SECTIONS)

    async def health(self) -> dict[str, Any]:
        return {
            "active": True,
            "serviceId": "svc_mock_3dprint",
            "serviceName": "3D-Print Terminal (Mock)",
            "status": "active",
            "reasonCode": "service_token_accepted",
        }

    async def get_metadata(self) -> dict[str, Any]:
        return {
            "service": {
                "serviceId": "svc_mock_3dprint",
                "name": "3D-Print Terminal (Mock)",
                "serviceTypeId": "svc_type_payment",
            },
            "sections": self._sections,
            "machineServices": [],
            "accounting": {
                "generalLedgerAccounts": [{"number": "4100", "name": "Materialverkauf 3D-Druck"}],
                "costCenters": [{"number": "300", "name": "3D-Druck", "sectionId": "sec_3d"}],
                "defaults": {"generalLedgerAccountNumber": "4100", "costCenterNumber": "300"},
            },
        }

    async def authorize(self, *, mode: str, rfid: str, pin: str | None = None) -> dict[str, Any]:
        uid = rfid.strip().upper()
        rec = self._users.get(uid)
        if not rec:
            return {
                "authorized": False,
                "serviceId": "svc_mock_3dprint",
                "serviceName": "3D-Print Terminal (Mock)",
                "status": "denied",
                "reason": "RFID_NOT_FOUND",
            }
        if mode == "rfid_pin":
            if not pin or pin.strip() != rec["pin"]:
                return {
                    "authorized": False,
                    "serviceId": "svc_mock_3dprint",
                    "serviceName": "3D-Print Terminal (Mock)",
                    "status": "denied",
                    "reason": "PIN_INVALID",
                }

        auth_id = f"pa_{uuid.uuid4().hex[:10]}"
        expires_in = 300
        self._authorizations[auth_id] = {
            "uid": uid,
            "consumed": False,
            "cancelled": False,
            "expires_at": time.time() + expires_in,
        }
        return {
            "authorized": True,
            "serviceId": "svc_mock_3dprint",
            "serviceName": "3D-Print Terminal (Mock)",
            "mode": mode,
            "paymentAuthorizationId": auth_id,
            "expiresInSeconds": expires_in,
            "user": {
                "member_id": rec["member_id"],
                "user_id": rec["user_id"],
                "display_name": rec["display_name"],
                "first_name": rec["first_name"],
                "last_name": rec["last_name"],
            },
            "balance": {"amount": rec["balance_cents"], "currency": "EUR"},
            "approved": rec["approved"],
            "is_instructor": rec["is_instructor"],
            "transactions": list(rec["transactions"]),
        }

    async def charge(
        self,
        *,
        payment_authorization_id: str,
        amount_cents: int,
        items: list[dict[str, Any]],
        idempotency_key: str,
        correlation_id: str,
        allow_overdraft: bool = False,
        max_overdraft_cents: int = 0,
    ) -> dict[str, Any]:
        if idempotency_key in self._idempotency:
            return self._idempotency[idempotency_key]

        auth = self._authorizations.get(payment_authorization_id)
        if not auth:
            return {"paid": False, "reason": "PAYMENT_AUTHORIZATION_NOT_FOUND"}
        if auth["cancelled"]:
            return {"paid": False, "reason": "PAYMENT_AUTHORIZATION_CANCELLED"}
        if auth["consumed"]:
            return {"paid": False, "reason": "PAYMENT_AUTHORIZATION_CONSUMED"}
        if time.time() >= auth["expires_at"]:
            return {"paid": False, "reason": "PAYMENT_AUTHORIZATION_EXPIRED"}

        rec = self._users[auth["uid"]]
        projected = rec["balance_cents"] - amount_cents
        if projected < 0:
            if not allow_overdraft:
                return {
                    "paid": False,
                    "paymentAuthorizationId": payment_authorization_id,
                    "reason": "INSUFFICIENT_BALANCE",
                    "balance": {"amount": rec["balance_cents"], "currency": "EUR"},
                }
            if max_overdraft_cents > 0 and projected < -max_overdraft_cents:
                return {
                    "paid": False,
                    "paymentAuthorizationId": payment_authorization_id,
                    "reason": "OVERDRAFT_LIMIT_EXCEEDED",
                    "balance": {"amount": rec["balance_cents"], "currency": "EUR"},
                }

        rec["balance_cents"] = projected
        auth["consumed"] = True
        title = items[0]["name"] if items else "3D-Druck"
        rec["transactions"].insert(0, {"ts": time.time(), "title": title, "amountCents": -amount_cents})

        res = {
            "paid": True,
            "paymentId": f"pay_{uuid.uuid4().hex[:10]}",
            "paymentAuthorizationId": payment_authorization_id,
            "amount": amount_cents,
            "currency": "EUR",
            "remainingBalance": {"amount": rec["balance_cents"], "currency": "EUR"},
            "correlationId": correlation_id,
        }
        self._idempotency[idempotency_key] = res
        return res

    async def cancel_authorization(self, *, payment_authorization_id: str, reason: str) -> dict[str, Any]:
        auth = self._authorizations.get(payment_authorization_id)
        if auth and not auth["consumed"]:
            auth["cancelled"] = True
        return {"cancelled": True, "paymentAuthorizationId": payment_authorization_id, "reason": reason}

    async def topup_mock(self, *, rfid: str, amount_cents: int) -> dict[str, Any]:
        uid = rfid.strip().upper()
        rec = self._users.get(uid)
        if not rec or amount_cents <= 0:
            return {"ok": False, "reason": "INVALID_TOPUP"}
        rec["balance_cents"] += amount_cents
        rec["transactions"].insert(0, {"ts": time.time(), "title": "Aufladung am Terminal", "amountCents": amount_cents})
        return {"ok": True, "balanceCents": rec["balance_cents"], "transactions": list(rec["transactions"])}

    async def approve_user_mock(self, *, target_rfid: str, instructor_rfid: str) -> dict[str, Any]:
        inst = self._users.get(instructor_rfid.strip().upper())
        target = self._users.get(target_rfid.strip().upper())
        if not inst or not inst.get("is_instructor"):
            return {"ok": False, "reason": "NOT_AUTHORIZED_INSTRUCTOR"}
        if not target:
            return {"ok": False, "reason": "TARGET_NOT_FOUND"}
        target["approved"] = True
        return {"ok": True, "instructorName": inst["display_name"]}


def _mask_uid(uid: str) -> str:
    clean = uid.strip().upper()
    if len(clean) <= 8:
        return clean
    return f"{clean[:4]}…{clean[-4:]}"


class CashboxPaymentProvider(PaymentProvider):
    """FREILab Pilot Mode (PAYMENT_MODE=cashbox): accepts any RFID card, asks for cashbox payment,
    logs every job with masked card UID + payment mode to SQLite, and gates BV Admin via ADMIN_BV_RFIDS.
    """

    def __init__(self, admin_bv_rfids: tuple[str, ...] = ()) -> None:
        self._bv_uids = {u.strip().upper() for u in admin_bv_rfids if u.strip()}
        self._sections = list(FREILAB_SECTIONS)

    async def health(self) -> dict[str, Any]:
        return {
            "active": True,
            "serviceId": "cashbox_pilot",
            "serviceName": "3D-Print Terminal (Pilot · Kasse)",
            "status": "active",
            "reasonCode": "cashbox_pilot_active",
        }

    async def get_metadata(self) -> dict[str, Any]:
        return {
            "service": {
                "serviceId": "cashbox_pilot",
                "name": "3D-Print Terminal (Pilot · Kasse)",
                "status": "active",
            },
            "sections": self._sections,
        }

    async def authorize(self, *, mode: str, rfid: str, pin: str | None = None) -> dict[str, Any]:
        uid = rfid.strip().upper()
        if len(uid) < 2:
            return {"authorized": False, "reason": "RFID_NOT_FOUND"}
        short_uid = _mask_uid(uid)
        is_bv = uid in self._bv_uids
        auth_id = f"cash_auth_{uuid.uuid4().hex[:12]}"
        return {
            "authorized": True,
            "mode": "cashbox",
            "paymentAuthorizationId": auth_id,
            "expiresInSeconds": 300,
            "user": {
                "member_id": f"rfid_{short_uid}",
                "user_id": f"rfid_{short_uid}",
                "display_name": f"BV ({short_uid})" if is_bv else f"Karte {short_uid}",
                "first_name": "Karte",
                "last_name": short_uid,
            },
            "balance": {"amount": 0, "currency": "EUR"},
            "approved": True,
            "is_instructor": is_bv,
            "transactions": [],
        }

    async def charge(
        self,
        *,
        payment_authorization_id: str,
        amount_cents: int,
        items: list[dict[str, Any]],
        idempotency_key: str,
        correlation_id: str,
        allow_overdraft: bool = False,
        max_overdraft_cents: int = 0,
    ) -> dict[str, Any]:
        return {
            "paid": True,
            "paymentId": f"cash_{uuid.uuid4().hex[:10]}",
            "paymentAuthorizationId": payment_authorization_id,
            "amount": amount_cents,
            "currency": "EUR",
            "remainingBalance": {"amount": 0, "currency": "EUR"},
            "correlationId": correlation_id,
        }

    async def cancel_authorization(self, *, payment_authorization_id: str, reason: str) -> dict[str, Any]:
        return {"cancelled": True, "paymentAuthorizationId": payment_authorization_id, "reason": reason}

    async def topup_mock(self, *, rfid: str, amount_cents: int) -> dict[str, Any]:
        return {"ok": False, "reason": "CASHBOX_MODE"}

    async def approve_user_mock(self, *, target_rfid: str, instructor_rfid: str) -> dict[str, Any]:
        return {"ok": True, "instructorName": "Pilot"}
