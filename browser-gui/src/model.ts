export type ViewAux = {
  title: string
}
export type WorkspaceInfo = {
  id: string
  views: string[]
  top_k: number
}
export type OutputInfo = {
  name: string
  max_views: number
}
export type ViewInfo = {
  tags: Record<string, 1>
}
export type State = {
  state: {
    workspaces: WorkspaceInfo[]
    outputs: OutputInfo[]
    views: Record<string, ViewInfo>
  }
  aux: {
    views: Record<string, ViewAux>
  }
}
