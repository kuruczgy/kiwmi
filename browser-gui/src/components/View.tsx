import { Draggable } from 'react-beautiful-dnd'
import { draggableProps } from '../utils'
import styles from './View.module.scss'
import ContentEditable from 'react-contenteditable'
import { useContext, useState } from 'react'
import { StateContext } from '../App'
import _ from 'lodash'

export const View = ({
  index,
  view_id,
}: {
  index: number
  view_id: string
}) => {
  const [state, setState] = useContext(StateContext)

  const view_info = state.state.views[view_id]
  const aux = state.aux.views[view_id]

  const tags = Object.keys(view_info.tags).sort()

  // TODO: not much point in being a controlled component really
  const [editState, setEditState] = useState({
    editing: null as string | null,
  })

  return (
    <Draggable draggableId={`draggable-view-${view_id}`} index={index}>
      {(provided, snapshot) => (
        <div
          {...draggableProps(provided, snapshot)}
          ref={provided.innerRef}
          className={styles.view}
        >
          <div>{aux.title}</div>
          <div className={styles.tags}>
            {tags.map((tag) => (
              <div
                onDoubleClick={() => {
                  setState((state) => {
                    state = _.cloneDeep(state)
                    delete state.state.views[view_id].tags[tag]
                    return state
                  })
                }}
                key={tag}
              >
                {tag}
              </div>
            ))}
            <ContentEditable
              html={editState.editing || ''}
              onChange={(e) => {
                const value = (e.nativeEvent.target as any).textContent
                setEditState((state) => ({ ...state, editing: value }))
              }}
              onKeyDown={(e) => {
                // https://github.com/lovasoa/react-contenteditable/issues/97#issuecomment-709640462
                const target = e.nativeEvent.target as any
                const value = target.textContent as string
                if (e.code === 'Enter') {
                  setState((state) => {
                    state = _.cloneDeep(state)
                    const tag = value.trim()
                    if (tag) state.state.views[view_id].tags[tag] = 1
                    return state
                  })
                  setEditState((editState) => ({ ...editState, editing: null }))
                  target.blur()
                  e.preventDefault()
                } else if (e.code === 'Escape') {
                  target.blur()
                }
              }}
              onBlur={() =>
                setEditState((editState) => ({ ...editState, editing: null }))
              }
            />
          </div>
        </div>
      )}
    </Draggable>
  )
}
