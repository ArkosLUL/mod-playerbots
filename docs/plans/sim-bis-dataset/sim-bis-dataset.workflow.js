export const meta = {
  name: 'pb-bis-wave',
  description: 'Sim BiS dataset effort: run one wave, implementing then independently reviewing each work item in its own worktree',
  whenToUse: 'Step 3 of the wave procedure in docs/plans/sim-bis-dataset/sim-bis-dataset.RUNBOOK.md',
  phases: [
    { title: 'Implement', detail: 'one implementer per work item, or one per stage' },
    { title: 'Review', detail: 'a fresh reviewer per work item fixes every finding' },
    { title: 'Repair', detail: 'one repair round for items still red, then a second review' },
  ],
}

// args: {wave, baseSha, runbook, items: [{id, worktree, branch, specPath, specSection, ownedPaths, verify,
// diffCmd?, notes?, priorReport?, stages?}]}
// diffCmd: how a reviewer lists the item's changes; defaults to `git -C <worktree> diff <baseSha>`.
// priorReport: a finished implementer's report from an earlier run; the item goes straight to review.
// stages: [{label, notes, priorReport?}], fresh implementers run in order in the same worktree, each handed
// the earlier stages' reports. A stage's priorReport skips that stage.

const STRINGS = { type: 'array', items: { type: 'string' } }

const REPORT = {
  type: 'object',
  properties: {
    status: { type: 'string', enum: ['green', 'red', 'blocked'] },
    summary: { type: 'string' },
    filesChanged: STRINGS,
    verification: {
      type: 'object',
      properties: { commands: STRINGS, passed: { type: 'boolean' }, tail: { type: 'string' } },
      required: ['commands', 'passed'],
    },
    contractChangeRequests: STRINGS,
    followUps: STRINGS,
    findings: {
      type: 'object',
      properties: { bugs: STRINGS, minor: STRINGS, cleanup: STRINGS },
    },
  },
  required: ['status', 'summary', 'filesChanged', 'verification'],
}

const diffCmd = item => item.diffCmd || `git -C ${item.worktree} diff ${args.baseSha}`

function brief(item, stage) {
  const stageList = !stage && item.stages
    ? `It ran in stages: ${item.stages.map(s => `"${s.label}": ${s.notes}`).join(' | ')}`
    : ''
  return [
    `Work item ${item.id}, wave ${args.wave} of the sim BiS dataset effort in the mod-playerbots module.`,
    item.branch
      ? `Worktree: ${item.worktree} (branch ${item.branch}, based on ${args.baseSha}). Work only there.`
      : `Checkout: ${item.worktree}. Work only there; its changes stay uncommitted for good.`,
    'Your shell starts in another checkout and resets there after every command: use absolute paths under your worktree for every file tool, and cd into it inside each Bash command. Use the Bash tool for git and the checks.',
    `Spec: ${item.specPath}, section "${item.specSection}". The same file's Decisions and Verified facts bind you.`,
    item.notes ? `Notes: ${item.notes}` : '',
    stage ? `Your stage, "${stage.label}": ${stage.notes}` : stageList,
    `Rules: the "Rules for WI agents" section of ${args.runbook}. Follow it exactly.`,
    `Owned paths: ${item.ownedPaths.join(', ')}.`,
    `Verification: ${item.verify.join(' ; ')}`,
    'Never build, restart or reconfigure the server, nor start or stop a Docker container; the DB is SELECT-only.',
    'No git writes: leave every change uncommitted.',
  ].filter(Boolean).join('\n')
}

const DONE_WHEN = 'Use status "blocked" for a problem the spec can\'t resolve.'

// Each stage is a fresh agent, so no single context has to hold the whole item.
async function implementStages(item) {
  const reports = []
  for (let i = 0; i < item.stages.length; i++) {
    const stage = item.stages[i]
    const earlier = reports.length
      ? `Earlier stages finished and left their changes uncommitted in the worktree; build on them and don't redo their work. Their reports:\n${JSON.stringify(reports)}`
      : 'You are the first stage.'
    let r = stage.priorReport || await agent(
      `${brief(item, stage)}\n\nThis item runs in ${item.stages.length} stages, each a fresh agent in the same worktree. You are stage ${i + 1}, "${stage.label}". ${earlier}\n\n` +
        `Do your stage only, then run the verification it can affect (the last stage runs all of it) and return the report. ${DONE_WHEN}`,
      { label: `impl:${item.id}:${stage.label}`, phase: 'Implement', schema: REPORT, effort: 'high' },
    )
    if (!r) return { status: 'red', incomplete: true, summary: `stage "${stage.label}" returned nothing`, stages: reports }
    // A priorReport may be a pointer to the report's file rather than the report itself.
    if (typeof r === 'string') r = { status: 'green', summary: r }
    reports.push({ stage: stage.label, ...r })
    if (r.status === 'blocked') return { status: 'blocked', summary: `stage "${stage.label}" is blocked`, stages: reports }
  }
  return {
    status: reports.some(r => r.status === 'red') ? 'red' : 'green',
    summary: reports.map(r => `[${r.stage}] ${r.summary}`).join('\n'),
    stages: reports,
  }
}

function implement(item) {
  if (item.priorReport) return item.priorReport
  if (item.stages) return implementStages(item)
  return agent(
    `${brief(item)}\n\nImplement the spec, then run its verification. Return the report. ${DONE_WHEN}`,
    { label: `impl:${item.id}`, phase: 'Implement', schema: REPORT, effort: 'high' },
  )
}

function review(item, prior, round) {
  return agent(
    `${brief(item)}\n\nYou are a fresh reviewer (round ${round}). The implementer reported:\n${JSON.stringify(prior)}\n\n` +
      `1. Review every change (${diffCmd(item)}, plus untracked files from git -C ${item.worktree} status --porcelain) by hand at high effort, against the spec and its Decisions. The code-review skill reviews the session's main checkout, not your worktree, so don't use it.\n` +
      '2. Fix every finding in the worktree, within the owned paths.\n' +
      '3. Re-run the verification.\n' +
      'Return the report with your findings grouped as bugs, minor and cleanup, and status "green" only if the verification passes.',
    { label: `review${round}:${item.id}`, phase: 'Review', schema: REPORT, effort: 'high' },
  )
}

function repair(item, failed) {
  return agent(
    `${brief(item)}\n\nThe review left this item red:\n${JSON.stringify(failed)}\n\nFix what it reports, re-run the verification, and return the report.`,
    { label: `repair:${item.id}`, phase: 'Repair', schema: REPORT, effort: 'high' },
  )
}

const settle = (item, report, status) => ({
  id: item.id,
  status: status || (report ? report.status : 'red'),
  report,
})

const results = await pipeline(
  args.items,
  item => implement(item),
  (impl, item) => {
    if (!impl) return { done: settle(item, null) }
    if (impl.incomplete) return { done: settle(item, impl, 'red') }
    if (impl.status === 'blocked') return { done: settle(item, impl) }
    return review(item, impl, 1).then(r => ({ review: r }))
  },
  async (prev, item) => {
    if (prev.done) return prev.done
    const first = prev.review
    if (!first || first.status !== 'red') return settle(item, first)
    const fixed = await repair(item, first)
    if (!fixed || fixed.status !== 'green') return settle(item, fixed)
    return settle(item, await review(item, fixed, 2))
  },
)

const out = results.map((r, i) => r || { id: args.items[i].id, status: 'red', report: null })
log(`Wave ${args.wave}: ${out.map(r => `${r.id}=${r.status}`).join(', ')}`)
return out
