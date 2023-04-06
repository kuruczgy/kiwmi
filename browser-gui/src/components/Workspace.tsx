import { Draggable, Droppable } from 'react-beautiful-dnd'
import type { WorkspaceInfo } from '../model'
import { draggableProps } from '../utils'
import styles from './Workspace.module.scss'
import { View } from './View'
import { NumberSelector } from './NumberSelector'
import { useContext } from 'react'
import { StateContext } from '../App'
import _ from 'lodash'

export const Workspace = ({
  ws,
  index,
}: {
  ws: WorkspaceInfo
  index: number
}) => {
  const [state, setState] = useContext(StateContext)
  return (
    <Draggable draggableId={`draggable-ws-${ws.id}`} index={index}>
      {(provided, snapshot) => (
        <div
          {...draggableProps(provided, snapshot)}
          ref={provided.innerRef}
          className={styles.workspace}
        >
          <div>
            <div>{ws.id}</div>
            <NumberSelector
              value={ws.top_k}
              onChange={(value) =>
                setState((state) => {
                  state = _.cloneDeep(state)
                  state.state.workspaces.find((i) => i.id === ws.id)!.top_k =
                    _.clamp(value, -1, 3)
                  return state
                })
              }
              label="top_k"
            />
          </div>
          <Droppable droppableId={`droppable-ws-${ws.id}`} type="view">
            {(provided) => (
              <div ref={provided.innerRef} {...provided.droppableProps}>
                {ws.views.map((view_id, index) => (
                  <View key={view_id} index={index} view_id={view_id} />
                ))}
                {provided.placeholder}
              </div>
            )}
          </Droppable>
        </div>
      )}
    </Draggable>
  )
}

export const NewWorkspace = () => {
  return (
    <div className={styles.workspace}>
      <div>[new workspace]</div>
      <Droppable droppableId="new-ws" type="view">
        {(provided) => (
          <div ref={provided.innerRef} {...provided.droppableProps}>
            {provided.placeholder}
          </div>
        )}
      </Droppable>
    </div>
  )
}
