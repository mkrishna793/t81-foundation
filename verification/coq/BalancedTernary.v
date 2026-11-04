(**
  Formal Mechanization of Balanced Ternary Arithmetic in Coq.
  Specification and Machine-Checked Proofs of Full Adder Correctness and Carry Bounds.
  Verified with Coq 8.18.
**)

Require Import ZArith.
Require Import Lia.
Require Import List.
Import ListNotations.
Open Scope Z_scope.

(** Inductive definition of a balanced trit: Neg (-1), Zero (0), Pos (+1) *)
Inductive trit : Type :=
  | Neg : trit
  | Zero : trit
  | Pos : trit.

(** Semantic interpretation of a trit as an integer in Z *)
Definition to_Z (t : trit) : Z :=
  match t with
  | Neg => -1
  | Zero => 0
  | Pos => 1
  end.

(** Balanced Ternary Full Adder: adds two trits and a carry-in *)
Definition full_adder (a b cin : trit) : (trit * trit) :=
  let total := to_Z a + to_Z b + to_Z cin in
  if total >=? 2 then
    (Pos, match (total - 3) with
          | -1 => Neg
          | 0 => Zero
          | _ => Pos
          end)
  else if total <=? -2 then
    (Neg, match (total + 3) with
          | -1 => Neg
          | 0 => Zero
          | _ => Pos
          end)
  else
    (Zero, match total with
           | -1 => Neg
           | 0 => Zero
           | _ => Pos
           end).

(** Helper lemma: Bounds of trit value in Z *)
Lemma to_Z_bounds : forall (t : trit), -1 <= to_Z t <= 1.
Proof.
  destruct t; simpl; lia.
Qed.

(** Theorem: Algebraic Correctness of the Balanced Ternary Full Adder *)
Theorem full_adder_correct : forall (a b cin : trit),
  let (cout, sum) := full_adder a b cin in
  to_Z a + to_Z b + to_Z cin = 3 * to_Z cout + to_Z sum.
Proof.
  intros a b cin.
  destruct a; destruct b; destruct cin; simpl; reflexivity.
Qed.

(** Theorem: Carry Out is strictly bounded within balanced trit range {-1, 0, 1} *)
Theorem carry_out_bounded : forall (a b cin : trit),
  let (cout, _) := full_adder a b cin in
  -1 <= to_Z cout <= 1.
Proof.
  intros a b cin.
  destruct a; destruct b; destruct cin; simpl; lia.
Qed.

(** Definition of Horner evaluation of a list of trits (LSB first) *)
Fixpoint eval_trits (ts : list trit) : Z :=
  match ts with
  | nil => 0
  | t :: rest => to_Z t + 3 * eval_trits rest
  end.

(** Theorem: Evaluation of empty list is 0 *)
Theorem eval_nil : eval_trits nil = 0.
Proof.
  reflexivity.
Qed.

(** Theorem: Evaluation of single trit *)
Theorem eval_single : forall (t : trit), eval_trits (t :: nil) = to_Z t.
Proof.
  intro t.
  simpl.
  lia.
Qed.
